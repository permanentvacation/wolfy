/*
 * WOLFY 3D - a Wolfenstein-3D-style raycasting shooter in C with SDL2.
 *
 * Everything (textures, sprites, sound effects, music, levels) is generated
 * procedurally at start-up, so there are no asset files to ship.
 *
 * Build:  make            Run: ./wolfy
 * Extras: ./wolfy --help
 */
#include <SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* constants                                                          */
/* ------------------------------------------------------------------ */
#define SCR_W 640
#define SCR_H 400
#define BAR_H 80
#define VIEW_H (SCR_H - BAR_H)
#define TEX 64
#define MAXMAP 64
#define MAXE 160
#define MAXI 400
#define NLEVELS 5
#define RATE 44100
#define PI_F 3.14159265f
#define TAU_F 6.28318531f

#define RGB(r, g, b) (0xFF000000u | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

/* ------------------------------------------------------------------ */
/* random numbers                                                     */
/* ------------------------------------------------------------------ */
static uint32_t rng_s = 12345;
static uint32_t rnd(void) {
    rng_s ^= rng_s << 13;
    rng_s ^= rng_s >> 17;
    rng_s ^= rng_s << 5;
    return rng_s;
}
static float frnd(void) { return (rnd() & 0xFFFFFF) / 16777216.0f; }
static int irnd(int n) { return (int)(rnd() % (uint32_t)n); }
static float noisef(void) { return frnd() * 2.0f - 1.0f; }
static uint32_t hash2(int x, int y, int s) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)s * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* ------------------------------------------------------------------ */
/* framebuffer + 3x5 font                                             */
/* ------------------------------------------------------------------ */
static uint32_t fb[SCR_W * SCR_H];

static inline void fpx(int x, int y, uint32_t c) {
    if ((unsigned)x < SCR_W && (unsigned)y < SCR_H) fb[y * SCR_W + x] = c;
}
static void frect(int x, int y, int w, int h, uint32_t c) {
    int x1 = x + w, y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > SCR_W) x1 = SCR_W;
    if (y1 > SCR_H) y1 = SCR_H;
    for (int j = y; j < y1; j++)
        for (int i = x; i < x1; i++) fb[j * SCR_W + i] = c;
}
static inline uint32_t blend(uint32_t a, uint32_t b, int al) { /* al 0..256 of b */
    int ia = 256 - al;
    uint32_t r = (((a >> 16) & 255) * ia + ((b >> 16) & 255) * al) >> 8;
    uint32_t g = (((a >> 8) & 255) * ia + ((b >> 8) & 255) * al) >> 8;
    uint32_t bl = ((a & 255) * ia + (b & 255) * al) >> 8;
    return 0xFF000000u | (r << 16) | (g << 8) | bl;
}
static void frect_a(int x, int y, int w, int h, uint32_t c, int al) {
    int x1 = x + w, y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > SCR_W) x1 = SCR_W;
    if (y1 > SCR_H) y1 = SCR_H;
    for (int j = y; j < y1; j++)
        for (int i = x; i < x1; i++) fb[j * SCR_W + i] = blend(fb[j * SCR_W + i], c, al);
}

static const char *glyph(int c) {
    if (c >= 'a' && c <= 'z') c -= 32;
    switch (c) {
    case '0': return "111101101101111";
    case '1': return "010110010010111";
    case '2': return "111001111100111";
    case '3': return "111001111001111";
    case '4': return "101101111001001";
    case '5': return "111100111001111";
    case '6': return "111100111101111";
    case '7': return "111001001010010";
    case '8': return "111101111101111";
    case '9': return "111101111001111";
    case 'A': return "010101111101101";
    case 'B': return "110101110101110";
    case 'C': return "011100100100011";
    case 'D': return "110101101101110";
    case 'E': return "111100110100111";
    case 'F': return "111100110100100";
    case 'G': return "011100101101011";
    case 'H': return "101101111101101";
    case 'I': return "111010010010111";
    case 'J': return "001001001101010";
    case 'K': return "101101110101101";
    case 'L': return "100100100100111";
    case 'M': return "101111111101101";
    case 'N': return "110101101101101";
    case 'O': return "010101101101010";
    case 'P': return "110101110100100";
    case 'Q': return "010101101110011";
    case 'R': return "110101110101101";
    case 'S': return "011100010001110";
    case 'T': return "111010010010010";
    case 'U': return "101101101101111";
    case 'V': return "101101101101010";
    case 'W': return "101101111111101";
    case 'X': return "101101010101101";
    case 'Y': return "101101010010010";
    case 'Z': return "111001010100111";
    case '-': return "000000111000000";
    case ':': return "000010000010000";
    case '!': return "010010010000010";
    case '.': return "000000000000010";
    case '/': return "001001010100100";
    case '%': return "101001010100101";
    case '?': return "111001011000010";
    case '+': return "000010111010000";
    case ',': return "000000000010100";
    case '\'': return "010010000000000";
    default: return NULL;
    }
}
static int text_w(const char *s, int sc) { return (int)strlen(s) * 4 * sc - sc; }
static void draw_text(int x, int y, int sc, uint32_t col, const char *s) {
    for (; *s; s++, x += 4 * sc) {
        const char *g = glyph(*s);
        if (!g) continue;
        for (int i = 0; i < 15; i++)
            if (g[i] == '1') frect(x + (i % 3) * sc, y + (i / 3) * sc, sc, sc, col);
    }
}
static void draw_text_sh(int x, int y, int sc, uint32_t col, const char *s) {
    draw_text(x + sc, y + sc, sc, RGB(0, 0, 0), s);
    draw_text(x, y, sc, col, s);
}
static void draw_text_c(int cx, int y, int sc, uint32_t col, const char *s) {
    draw_text_sh(cx - text_w(s, sc) / 2, y, sc, col, s);
}

/* ------------------------------------------------------------------ */
/* procedural textures (column-major: t[x*TEX + y])                   */
/* ------------------------------------------------------------------ */
enum { T_EMPTY = 0, T_STONE, T_BLUE, T_WOOD, T_RED, T_DOOR, T_EXIT };
enum {
    SP_GUARD = 0, /* 6 poses each: stand walk1 walk2 shoot pain dead */
    SP_SS = 6,
    SP_BOSS = 12,
    SP_HEALTH = 18,
    SP_AMMO,
    SP_TREAS1,
    SP_TREAS2,
    SP_GUN,
    SP_PILLAR,
    NSPR
};
enum { W_KNIFE = 0, W_PISTOL, W_MG, NWPN };

static uint32_t walltex[6][TEX * TEX];
static uint32_t sprtex[NSPR][TEX * TEX];
static uint32_t wpntex[NWPN * 2][TEX * TEX];

static inline void tpx(uint32_t *t, int x, int y, uint32_t c) {
    if ((unsigned)x < TEX && (unsigned)y < TEX) t[x * TEX + y] = c;
}
static void trect(uint32_t *t, int x0, int y0, int x1, int y1, uint32_t c) {
    for (int x = x0; x < x1; x++)
        for (int y = y0; y < y1; y++) tpx(t, x, y, c);
}
static void tell(uint32_t *t, int cx, int cy, int rx, int ry, uint32_t c) {
    for (int y = cy - ry; y <= cy + ry; y++)
        for (int x = cx - rx; x <= cx + rx; x++) {
            float dx = (float)(x - cx) / (rx + 0.5f), dy = (float)(y - cy) / (ry + 0.5f);
            if (dx * dx + dy * dy <= 1.0f) tpx(t, x, y, c);
        }
}
static void tline(uint32_t *t, int x0, int y0, int x1, int y1, int th, uint32_t c) {
    int n = abs(x1 - x0) > abs(y1 - y0) ? abs(x1 - x0) : abs(y1 - y0);
    if (n == 0) n = 1;
    for (int i = 0; i <= n; i++) {
        int x = x0 + (x1 - x0) * i / n, y = y0 + (y1 - y0) * i / n;
        trect(t, x - th / 2, y - th / 2, x - th / 2 + th, y - th / 2 + th, c);
    }
}
static uint32_t scale_c(uint32_t c, float f) {
    int r = (int)(((c >> 16) & 255) * f), g = (int)(((c >> 8) & 255) * f), b = (int)((c & 255) * f);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return (c & 0xFF000000u) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}
static uint32_t jit(uint32_t c, int x, int y, int seed, float amt) {
    float n = ((hash2(x, y, seed) & 255) / 255.0f - 0.5f) * 2.0f * amt;
    return scale_c(c, 1.0f + n);
}
static void ttext(uint32_t *t, int x, int y, int sc, uint32_t col, const char *s) {
    for (; *s; s++, x += 4 * sc) {
        const char *g = glyph(*s);
        if (!g) continue;
        for (int i = 0; i < 15; i++)
            if (g[i] == '1') trect(t, x + (i % 3) * sc, y + (i / 3) * sc, x + (i % 3) * sc + sc, y + (i / 3) * sc + sc, col);
    }
}
static void toutline(uint32_t *t, uint32_t col) {
    static uint32_t tmp[TEX * TEX];
    memcpy(tmp, t, sizeof tmp);
    for (int x = 0; x < TEX; x++)
        for (int y = 0; y < TEX; y++) {
            if (tmp[x * TEX + y] >> 24) continue;
            int near = 0;
            for (int dx = -1; dx <= 1 && !near; dx++)
                for (int dy = -1; dy <= 1; dy++) {
                    int a = x + dx, b = y + dy;
                    if ((unsigned)a < TEX && (unsigned)b < TEX && (tmp[a * TEX + b] >> 24)) { near = 1; break; }
                }
            if (near) t[x * TEX + y] = col;
        }
}

static void gen_brick(uint32_t *t, uint32_t base, uint32_t mortar, int bw, int bh, int seed) {
    for (int y = 0; y < TEX; y++)
        for (int x = 0; x < TEX; x++) {
            int row = y / bh, off = (row & 1) ? bw / 2 : 0;
            int xx = (x + off) % TEX, bx = xx % bw, by = y % bh;
            int id = xx / bw + row * 17;
            uint32_t c;
            if (bx == 0 || by == 0) c = jit(mortar, x, y, seed, 0.12f);
            else {
                c = scale_c(base, 0.85f + (hash2(id, 3, seed) & 63) / 200.0f);
                c = jit(c, x, y, seed + 1, 0.10f);
                if (bx == 1 || by == 1) c = scale_c(c, 1.18f);
                if (bx == bw - 1 || by == bh - 1) c = scale_c(c, 0.78f);
            }
            t[x * TEX + y] = c;
        }
}
static void gen_wood(uint32_t *t) {
    for (int y = 0; y < TEX; y++)
        for (int x = 0; x < TEX; x++) {
            int plank = x / 16, bx = x % 16;
            float g = 0.82f + 0.18f * sinf(y * 0.35f + plank * 2.7f + sinf(x * 0.9f + plank) * 1.5f);
            uint32_t c = scale_c(RGB(150, 98, 52), g);
            c = jit(c, x, y, 7, 0.06f);
            if (bx == 0 || bx == 15) c = scale_c(c, 0.45f);
            else if (bx == 1) c = scale_c(c, 1.15f);
            if (y % 32 == 0 && bx % 8 < 2) c = scale_c(c, 0.6f); /* nail-ish marks */
            t[x * TEX + y] = c;
        }
}
static void gen_door(uint32_t *t) {
    for (int y = 0; y < TEX; y++)
        for (int x = 0; x < TEX; x++) {
            uint32_t c = jit(RGB(118, 124, 138), x, y, 11, 0.05f);
            if (y % 16 == 0) c = scale_c(c, 0.55f);
            else if (y % 16 == 1) c = scale_c(c, 1.2f);
            if (x == 31 || x == 32) c = scale_c(c, 0.4f);
            if (x < 3 || x > 60 || y < 3 || y > 60) c = scale_c(c, 0.65f);
            if ((x < 3 || x > 60) && ((x + y) / 6) % 2 == 0) c = RGB(200, 170, 20);
            t[x * TEX + y] = c;
        }
    tell(t, 24, 34, 3, 3, RGB(230, 200, 40));
    tell(t, 40, 34, 3, 3, RGB(230, 200, 40));
}
static void gen_exit(uint32_t *t) {
    for (int y = 0; y < TEX; y++)
        for (int x = 0; x < TEX; x++) {
            uint32_t c = jit(RGB(30, 110, 50), x, y, 21, 0.06f);
            if (x < 6 || x > 57 || y < 6 || y > 57) c = (((x + y) / 7) % 2) ? RGB(230, 190, 20) : RGB(30, 30, 30);
            t[x * TEX + y] = c;
        }
    ttext(t, 9, 14, 3, RGB(245, 245, 235), "EXIT");
    /* big arrow */
    trect(t, 24, 36, 40, 44, RGB(245, 245, 235));
    for (int i = 0; i < 8; i++) trect(t, 28 + i, 44 + i, 36 - i, 45 + i, RGB(245, 245, 235));
}
static void gen_walls(void) {
    gen_brick(walltex[0], RGB(120, 120, 128), RGB(55, 55, 60), 32, 16, 1);
    gen_brick(walltex[1], RGB(60, 84, 160), RGB(22, 28, 60), 32, 16, 2);
    gen_wood(walltex[2]);
    gen_brick(walltex[3], RGB(150, 58, 44), RGB(60, 30, 26), 16, 8, 3);
    gen_door(walltex[4]);
    gen_exit(walltex[5]);
}

/* sprites */
typedef struct { uint32_t uni, uni2, helm, skin, belt, gun; } Pal;

static void gen_soldier(uint32_t *t, int pose, Pal p) {
    memset(t, 0, sizeof(uint32_t) * TEX * TEX);
    if (pose == 5) { /* dead */
        tell(t, 32, 58, 27, 4, RGB(110, 14, 14));
        trect(t, 10, 47, 44, 57, p.uni);
        trect(t, 44, 49, 60, 56, p.uni2);
        trect(t, 58, 49, 63, 56, RGB(20, 20, 20));
        tell(t, 8, 52, 7, 6, p.skin);
        tell(t, 8, 49, 7, 4, p.helm);
        trect(t, 14, 51, 34, 54, p.gun);
        trect(t, 10, 52, 44, 53, p.belt);
        tell(t, 24, 50, 3, 2, RGB(150, 20, 20));
    } else {
        int lo = pose == 1 ? 3 : (pose == 2 ? -3 : 0);
        int hx = 0, hy = 0;
        if (pose == 4) { hx = 2; hy = -1; }
        /* legs */
        trect(t, 23, 44, 31, 62 - (lo > 0 ? lo : 0), p.uni2);
        trect(t, 33, 44, 41, 62 - (lo < 0 ? -lo : 0), p.uni2);
        trect(t, 22, 58 - (lo > 0 ? lo : 0), 32, 62 - (lo > 0 ? lo : 0) + 1, RGB(25, 20, 18));
        trect(t, 32, 58 - (lo < 0 ? -lo : 0), 42, 62 - (lo < 0 ? -lo : 0) + 1, RGB(25, 20, 18));
        /* torso */
        trect(t, 21, 20, 43, 45, p.uni);
        trect(t, 21, 38, 43, 41, p.belt);
        trect(t, 30, 38, 34, 41, RGB(220, 190, 40));
        trect(t, 31, 20, 33, 38, scale_c(p.uni, 0.8f));
        /* arms */
        if (pose == 4) {
            trect(t, 12, 12, 20, 34, p.uni);
            trect(t, 12, 8, 20, 12, p.skin);
            trect(t, 44, 12, 52, 34, p.uni);
            trect(t, 44, 8, 52, 12, p.skin);
        } else {
            trect(t, 15, 22, 21, 40, p.uni);
            trect(t, 15, 40, 21, 44, p.skin);
            if (pose == 3) {
                trect(t, 43, 24, 51, 34, p.uni);
                trect(t, 39, 27, 52, 36, p.gun);
                trect(t, 36, 29, 40, 33, p.skin);
            } else {
                trect(t, 43, 22, 49, 40, p.uni);
                trect(t, 43, 40, 49, 44, p.skin);
                trect(t, 46, 38, 52, 54, p.gun);
            }
        }
        /* head */
        tell(t, 32 + hx, 13 + hy, 7, 8, p.skin);
        tell(t, 32 + hx, 9 + hy, 8, 6, p.helm);
        trect(t, 24 + hx, 9 + hy, 41 + hx, 12 + hy, p.helm);
        if (pose == 4) {
            trect(t, 28 + hx, 13 + hy, 31 + hx, 14 + hy, RGB(30, 20, 20));
            trect(t, 34 + hx, 13 + hy, 37 + hx, 14 + hy, RGB(30, 20, 20));
            tell(t, 32 + hx, 19 + hy, 2, 2, RGB(140, 20, 20));
        } else {
            tpx(t, 29, 14, RGB(25, 20, 20));
            tpx(t, 35, 14, RGB(25, 20, 20));
            trect(t, 30, 18, 35, 19, RGB(110, 50, 45));
        }
        if (pose == 3) { /* muzzle flash */
            tell(t, 37, 27, 7, 6, RGB(255, 200, 40));
            tell(t, 37, 27, 4, 3, RGB(255, 255, 220));
        }
    }
    /* shading + noise */
    for (int x = 0; x < TEX; x++)
        for (int y = 0; y < TEX; y++) {
            uint32_t c = t[x * TEX + y];
            if (!(c >> 24)) continue;
            float f = 1.12f - 0.30f * x / 63.0f;
            t[x * TEX + y] = jit(scale_c(c, f), x, y, 33, 0.05f);
        }
    toutline(t, RGB(10, 8, 8));
}

static void gen_sprites(void) {
    Pal guard = { RGB(120, 100, 60), RGB(95, 80, 50), RGB(70, 70, 60), RGB(225, 180, 140), RGB(60, 40, 25), RGB(40, 40, 45) };
    Pal ss = { RGB(50, 70, 140), RGB(40, 50, 100), RGB(30, 30, 40), RGB(235, 195, 160), RGB(20, 20, 25), RGB(30, 30, 35) };
    Pal boss = { RGB(235, 235, 240), RGB(150, 40, 40), RGB(180, 40, 40), RGB(235, 190, 150), RGB(200, 160, 30), RGB(60, 60, 70) };
    for (int i = 0; i < 6; i++) {
        gen_soldier(sprtex[SP_GUARD + i], i, guard);
        gen_soldier(sprtex[SP_SS + i], i, ss);
        gen_soldier(sprtex[SP_BOSS + i], i, boss);
    }
    uint32_t *t;
    /* health kit */
    t = sprtex[SP_HEALTH];
    memset(t, 0, sizeof(uint32_t) * TEX * TEX);
    trect(t, 16, 38, 48, 58, RGB(235, 235, 235));
    trect(t, 16, 54, 48, 58, RGB(170, 170, 175));
    trect(t, 29, 41, 35, 55, RGB(210, 20, 20));
    trect(t, 24, 46, 40, 50, RGB(210, 20, 20));
    trect(t, 26, 34, 38, 38, RGB(190, 190, 195));
    toutline(t, RGB(10, 10, 10));
    /* ammo clip */
    t = sprtex[SP_AMMO];
    memset(t, 0, sizeof(uint32_t) * TEX * TEX);
    trect(t, 18, 46, 46, 58, RGB(90, 90, 100));
    trect(t, 18, 46, 46, 49, RGB(130, 130, 140));
    for (int i = 0; i < 4; i++) {
        trect(t, 21 + i * 6, 36, 25 + i * 6, 47, RGB(220, 180, 50));
        trect(t, 21 + i * 6, 32, 25 + i * 6, 37, RGB(160, 100, 40));
    }
    toutline(t, RGB(10, 10, 10));
    /* treasure 1: chalice */
    t = sprtex[SP_TREAS1];
    memset(t, 0, sizeof(uint32_t) * TEX * TEX);
    for (int y = 30; y < 44; y++)
        for (int x = 20; x < 44; x++) {
            float dx = (x - 32) / 12.0f, dy = (y - 30) / 14.0f;
            if (dx * dx + dy * dy <= 1.0f) tpx(t, x, y, RGB(240, 200, 40));
        }
    trect(t, 20, 28, 44, 32, RGB(255, 225, 80));
    trect(t, 30, 44, 34, 54, RGB(210, 165, 30));
    trect(t, 22, 54, 42, 59, RGB(240, 200, 40));
    tell(t, 27, 36, 2, 3, RGB(255, 245, 160));
    toutline(t, RGB(10, 10, 10));
    /* treasure 2: jeweled cross */
    t = sprtex[SP_TREAS2];
    memset(t, 0, sizeof(uint32_t) * TEX * TEX);
    trect(t, 28, 26, 36, 60, RGB(240, 200, 40));
    trect(t, 20, 33, 44, 41, RGB(240, 200, 40));
    trect(t, 28, 26, 30, 60, RGB(255, 230, 100));
    tell(t, 32, 37, 2, 2, RGB(210, 30, 40));
    tell(t, 32, 28, 1, 1, RGB(40, 120, 220));
    tell(t, 22, 37, 1, 1, RGB(40, 200, 90));
    tell(t, 42, 37, 1, 1, RGB(40, 200, 90));
    toutline(t, RGB(10, 10, 10));
    /* machine gun pickup */
    t = sprtex[SP_GUN];
    memset(t, 0, sizeof(uint32_t) * TEX * TEX);
    trect(t, 8, 46, 56, 52, RGB(70, 72, 80));
    trect(t, 14, 42, 40, 48, RGB(50, 52, 60));
    trect(t, 38, 48, 46, 60, RGB(40, 42, 50));
    trect(t, 18, 50, 26, 58, RGB(110, 70, 40));
    trect(t, 8, 46, 56, 47, RGB(120, 124, 135));
    toutline(t, RGB(10, 10, 10));
    /* pillar */
    t = sprtex[SP_PILLAR];
    memset(t, 0, sizeof(uint32_t) * TEX * TEX);
    for (int x = 20; x < 44; x++)
        for (int y = 4; y < 64; y++) {
            float f = 0.55f + 0.6f * cosf((x - 28) / 24.0f * 1.4f);
            uint32_t c = jit(scale_c(RGB(150, 148, 140), f), x, y, 5, 0.06f);
            if (y < 9 || y > 58) c = scale_c(c, 1.15f);
            t[x * TEX + y] = c;
        }
    trect(t, 17, 2, 47, 8, RGB(165, 162, 155));
    trect(t, 17, 58, 47, 64, RGB(165, 162, 155));
    toutline(t, RGB(10, 10, 10));
}

static void gen_weapons(void) {
    uint32_t sleeve = RGB(80, 100, 60), skin = RGB(225, 180, 140);
    for (int i = 0; i < NWPN * 2; i++) memset(wpntex[i], 0, sizeof(uint32_t) * TEX * TEX);
    uint32_t *t;
    /* knife idle / swing */
    t = wpntex[0];
    tline(t, 38, 46, 46, 12, 4, RGB(205, 210, 220));
    tline(t, 38, 46, 46, 12, 1, RGB(255, 255, 255));
    trect(t, 33, 44, 47, 48, RGB(90, 70, 40));
    trect(t, 34, 48, 46, 58, RGB(60, 40, 25));
    trect(t, 30, 50, 50, 62, skin);
    trect(t, 24, 60, 56, 64, sleeve);
    t = wpntex[1];
    tline(t, 28, 44, 10, 18, 4, RGB(205, 210, 220));
    tline(t, 28, 44, 10, 18, 1, RGB(255, 255, 255));
    trect(t, 24, 42, 36, 48, RGB(90, 70, 40));
    trect(t, 24, 46, 34, 56, RGB(60, 40, 25));
    trect(t, 20, 48, 42, 60, skin);
    trect(t, 14, 58, 48, 64, sleeve);
    /* pistol */
    for (int f = 0; f < 2; f++) {
        t = wpntex[2 + f];
        trect(t, 29, 12, 35, 34, RGB(60, 60, 68));
        trect(t, 29, 12, 31, 34, RGB(110, 110, 120));
        trect(t, 27, 30, 37, 50, RGB(88, 88, 98));
        trect(t, 27, 30, 29, 50, RGB(140, 140, 150));
        trect(t, 26, 46, 38, 60, skin);
        trect(t, 22, 58, 42, 64, sleeve);
        if (f) {
            tell(t, 32, 8, 9, 8, RGB(255, 190, 40));
            tell(t, 32, 8, 5, 5, RGB(255, 255, 210));
        }
    }
    /* machine gun */
    for (int f = 0; f < 2; f++) {
        t = wpntex[4 + f];
        trect(t, 30, 2, 34, 28, RGB(55, 55, 62));
        for (int y = 6; y < 26; y += 4) trect(t, 28, y, 36, y + 1, RGB(25, 25, 30));
        trect(t, 25, 26, 39, 48, RGB(70, 72, 82));
        trect(t, 25, 26, 27, 48, RGB(120, 124, 135));
        trect(t, 35, 44, 42, 58, RGB(45, 45, 52));
        trect(t, 24, 46, 36, 60, skin);
        trect(t, 20, 58, 42, 64, sleeve);
        if (f) {
            tell(t, 32, 0, 11, 9, RGB(255, 190, 40));
            tell(t, 32, 0, 6, 6, RGB(255, 255, 210));
        }
    }
    for (int i = 0; i < NWPN * 2; i++) toutline(wpntex[i], RGB(10, 10, 10));
}

/* ------------------------------------------------------------------ */
/* audio: synthesised sound effects + music                           */
/* ------------------------------------------------------------------ */
enum {
    SND_PISTOL, SND_MG, SND_KNIFE, SND_ESHOT, SND_ALERT, SND_DEATH, SND_PAIN, SND_HURT,
    SND_DOOR, SND_DOORC, SND_AMMO, SND_HEALTH, SND_TREASURE, SND_GUNGET, SND_EXIT, SND_GAMEOVER,
    SND_EMPTY, SND_MENU, SND_BOSSDIE, SND_COUNT
};
typedef struct { int16_t *d; int n; } Sound;
static Sound snd[SND_COUNT];

typedef struct { const Sound *s; int pos; float vl, vr; } Voice;
#define NVOICE 24
static Voice voices[NVOICE];

static int16_t *music[2];
static int music_n[2];
static int mus_cur = -1, mus_pos = 0;
static float mus_vol = 0.7f, sfx_vol = 0.9f;
static int music_on = 1;
static SDL_AudioDeviceID adev = 0;

static float sqw(float ph) { return (ph - floorf(ph)) < 0.5f ? 1.0f : -1.0f; }
static float saw(float ph) { return 2.0f * (ph - floorf(ph)) - 1.0f; }
static float tri(float ph) { float p = ph - floorf(ph); return 4.0f * fabsf(p - 0.5f) - 1.0f; }
static float midi_hz(float m) { return 440.0f * powf(2.0f, (m - 69.0f) / 12.0f); }

static void finish_sound(int id, float *b, int n, float gain) {
    float peak = 0.001f;
    for (int i = 0; i < n; i++) if (fabsf(b[i]) > peak) peak = fabsf(b[i]);
    snd[id].d = malloc(sizeof(int16_t) * (size_t)n);
    snd[id].n = n;
    for (int i = 0; i < n; i++) {
        float v = b[i] / peak * gain;
        int fade = n - i < 64 ? n - i : 64; /* avoid click at the end */
        v *= fade / 64.0f > 1.0f ? 1.0f : fade / 64.0f;
        snd[id].d[i] = (int16_t)(clampf(v, -1.0f, 1.0f) * 30000.0f);
    }
    free(b);
}

static void gen_sound(int id, float dur, float gain) {
    int n = (int)(dur * RATE);
    float *b = calloc((size_t)n, sizeof(float));
    float ph = 0, ph2 = 0, lp = 0, lp2 = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / RATE, v = 0, nz = noisef();
        switch (id) {
        case SND_PISTOL: {
            lp += (nz - lp) * 0.55f;
            ph += (170.0f * expf(-t * 16.0f) + 45.0f) / RATE;
            v = lp * expf(-t * 22.0f) * 1.0f + sinf(TAU_F * ph) * expf(-t * 12.0f) * 0.8f;
            break;
        }
        case SND_MG: {
            lp += (nz - lp) * 0.7f;
            ph += (220.0f * expf(-t * 30.0f) + 70.0f) / RATE;
            v = lp * expf(-t * 40.0f) + sinf(TAU_F * ph) * expf(-t * 25.0f) * 0.9f;
            break;
        }
        case SND_KNIFE: {
            float a = sinf(PI_F * t / dur);
            float k = 0.05f + 0.5f * t / dur;
            lp += (nz - lp) * k;
            v = lp * a * a;
            break;
        }
        case SND_ESHOT: {
            lp += (nz - lp) * 0.35f;
            ph += (120.0f * expf(-t * 10.0f) + 38.0f) / RATE;
            v = lp * expf(-t * 15.0f) + saw(ph) * expf(-t * 9.0f) * 0.45f;
            break;
        }
        case SND_ALERT: { /* two-syllable robotic bark */
            float tt = t < 0.2f ? t : t - 0.21f;
            float env = (t < 0.18f || (t > 0.21f && t < 0.4f)) ? sinf(PI_F * tt / 0.19f) : 0.0f;
            if (env < 0) env = 0;
            float f = t < 0.2f ? 150.0f - 40.0f * t : 120.0f - 30.0f * (t - 0.2f);
            ph += f / RATE;
            float s = saw(ph) + 0.4f * sqw(ph * 2.0f);
            lp += (s - lp) * (t < 0.2f ? 0.28f : 0.18f);
            v = lp * env;
            break;
        }
        case SND_DEATH:
        case SND_BOSSDIE: {
            float f = (id == SND_BOSSDIE ? 170.0f : 280.0f) * expf(-t * 1.6f) + 55.0f;
            f *= 1.0f + 0.08f * sinf(TAU_F * 9.0f * t);
            ph += f / RATE;
            float s = saw(ph) * 0.8f + nz * 0.25f;
            lp += (s - lp) * (0.35f * expf(-t * 1.2f) + 0.06f);
            float env = fminf(1.0f, t / 0.02f) * expf(-t * 2.8f);
            v = lp * env;
            if (t < 0.04f) v += nz * 0.3f;
            break;
        }
        case SND_PAIN: {
            ph += (330.0f - 330.0f * t) / RATE;
            lp += (saw(ph) - lp) * 0.25f;
            v = lp * sinf(PI_F * t / dur);
            break;
        }
        case SND_HURT: {
            ph += (95.0f * expf(-t * 6.0f) + 40.0f) / RATE;
            lp += (nz - lp) * 0.12f;
            v = sinf(TAU_F * ph) * expf(-t * 9.0f) + lp * expf(-t * 14.0f) * 0.8f;
            break;
        }
        case SND_DOOR:
        case SND_DOORC: {
            float a = sinf(PI_F * t / dur);
            lp += (nz - lp) * 0.04f;
            lp2 += (lp - lp2) * 0.2f;
            ph += (50.0f + 20.0f * a) / RATE;
            v = (lp2 * 5.0f + sinf(TAU_F * ph) * 0.4f + sqw(ph * 3.0f) * 0.05f) * a;
            if (t < 0.03f) v += nz * 0.5f * (1.0f - t / 0.03f);
            if (id == SND_DOORC && t > dur - 0.12f) v += nz * 0.4f * expf(-(t - (dur - 0.12f)) * 40.0f);
            break;
        }
        case SND_AMMO: {
            static const float nt[3] = { 72, 76, 79 };
            int k = (int)(t / 0.07f);
            if (k > 2) k = 2;
            ph += midi_hz(nt[k]) / RATE;
            v = sqw(ph) * 0.5f * expf(-(t - k * 0.07f) * 8.0f) * (t < 0.21f ? 1.0f : expf(-(t - 0.21f) * 12.0f));
            break;
        }
        case SND_HEALTH: {
            float f = midi_hz(t < 0.12f ? 69.0f : 76.0f);
            ph += f / RATE;
            v = (sinf(TAU_F * ph) + 0.3f * sinf(TAU_F * ph * 2.0f)) * expf(-(t < 0.12f ? t : t - 0.12f) * 6.0f);
            break;
        }
        case SND_TREASURE: {
            static const float nt[4] = { 84, 88, 91, 96 };
            int k = (int)(t / 0.08f);
            if (k > 3) k = 3;
            float lt = t - k * 0.08f;
            ph += midi_hz(nt[k]) / RATE;
            v = (sinf(TAU_F * ph) + 0.5f * sinf(TAU_F * ph * 2.01f)) * expf(-lt * 5.0f);
            break;
        }
        case SND_GUNGET: {
            static const float nt[4] = { 55, 62, 67, 74 };
            int k = (int)(t / 0.1f);
            if (k > 3) k = 3;
            ph += midi_hz(nt[k]) / RATE;
            v = (sqw(ph) * 0.4f + saw(ph) * 0.4f) * expf(-(t - k * 0.1f) * 4.0f);
            if (k == 0 && t < 0.03f) v += nz * 0.5f;
            break;
        }
        case SND_EXIT: {
            static const float nt[5] = { 60, 64, 67, 72, 76 };
            int k = (int)(t / 0.17f);
            if (k > 4) k = 4;
            float lt = t - k * 0.17f;
            ph += midi_hz(nt[k]) / RATE;
            ph2 += midi_hz(nt[k] - 12) / RATE;
            float env = k < 4 ? expf(-lt * 3.0f) : expf(-lt * 1.5f);
            v = (sqw(ph) * 0.4f + saw(ph2) * 0.4f) * env;
            break;
        }
        case SND_GAMEOVER: {
            static const float nt[5] = { 69, 67, 65, 64, 57 };
            int k = (int)(t / 0.3f);
            if (k > 4) k = 4;
            float lt = t - k * 0.3f;
            ph += midi_hz(nt[k]) / RATE;
            ph2 += midi_hz(nt[k] - 12) / RATE;
            v = (saw(ph) * 0.5f + sqw(ph2) * 0.4f) * expf(-lt * (k < 4 ? 3.0f : 1.2f));
            break;
        }
        case SND_EMPTY:
            v = nz * expf(-t * 90.0f) * 0.6f + sinf(TAU_F * 1800.0f * t) * expf(-t * 120.0f) * 0.4f;
            break;
        case SND_MENU:
            ph += (t < 0.07f ? 660.0f : 880.0f) / RATE;
            v = sqw(ph) * 0.5f * expf(-(t < 0.07f ? t : t - 0.07f) * 14.0f);
            break;
        }
        b[i] = v;
    }
    finish_sound(id, b, n, gain);
}

static void gen_sounds(void) {
    gen_sound(SND_PISTOL, 0.35f, 0.95f);
    gen_sound(SND_MG, 0.18f, 0.9f);
    gen_sound(SND_KNIFE, 0.22f, 0.6f);
    gen_sound(SND_ESHOT, 0.45f, 0.9f);
    gen_sound(SND_ALERT, 0.42f, 0.75f);
    gen_sound(SND_DEATH, 0.9f, 0.85f);
    gen_sound(SND_BOSSDIE, 1.3f, 0.95f);
    gen_sound(SND_PAIN, 0.22f, 0.7f);
    gen_sound(SND_HURT, 0.35f, 0.95f);
    gen_sound(SND_DOOR, 0.9f, 0.7f);
    gen_sound(SND_DOORC, 0.7f, 0.7f);
    gen_sound(SND_AMMO, 0.4f, 0.7f);
    gen_sound(SND_HEALTH, 0.55f, 0.75f);
    gen_sound(SND_TREASURE, 0.65f, 0.7f);
    gen_sound(SND_GUNGET, 0.6f, 0.8f);
    gen_sound(SND_EXIT, 1.6f, 0.75f);
    gen_sound(SND_GAMEOVER, 1.9f, 0.75f);
    gen_sound(SND_EMPTY, 0.1f, 0.7f);
    gen_sound(SND_MENU, 0.2f, 0.6f);
}

/* ----- music ----- */
enum { INST_LEAD, INST_BASS, INST_PAD, INST_ARP };

static void add_note(float *buf, int n, int start, float dur, float freq, int inst, float vol) {
    float rel = inst == INST_PAD ? 0.35f : 0.05f;
    int len = (int)((dur + rel) * RATE);
    float ph = 0, ph2 = 0, ph3 = 0;
    for (int i = 0; i < len; i++) {
        float t = (float)i / RATE, env, v;
        switch (inst) {
        case INST_LEAD: {
            float f = freq * (1.0f + (t > 0.09f ? 0.006f * sinf(TAU_F * 5.5f * t) : 0.0f));
            ph += f / RATE;
            v = ((ph - floorf(ph)) < 0.3f ? 1.0f : -1.0f) * 0.7f + tri(ph) * 0.3f;
            env = fminf(1.0f, t / 0.008f) * (0.62f + 0.38f * expf(-t * 11.0f));
            break;
        }
        case INST_BASS:
            ph += freq / RATE;
            v = tri(ph) * 0.7f + sqw(ph) * 0.25f + sinf(TAU_F * ph * 0.5f) * 0.3f;
            env = fminf(1.0f, t / 0.004f) * (0.35f + 0.65f * expf(-t * 7.0f));
            break;
        case INST_PAD:
            ph += freq / RATE;
            ph2 += freq * 1.004f / RATE;
            ph3 += freq * 0.996f / RATE;
            v = (saw(ph) + saw(ph2) + saw(ph3)) * 0.3f * (0.6f + 0.4f * sinf(TAU_F * 0.4f * t));
            env = fminf(1.0f, t / 0.25f);
            break;
        default: /* ARP */
            ph += freq / RATE;
            v = ((ph - floorf(ph)) < 0.125f ? 1.0f : -1.0f);
            env = fminf(1.0f, t / 0.003f) * expf(-t * 14.0f);
            break;
        }
        if (t > dur) env *= fmaxf(0.0f, 1.0f - (t - dur) / rel);
        buf[(start + i) % n] += v * env * vol;
    }
}
static void add_kick(float *buf, int n, int start, float vol) {
    float ph = 0;
    int len = (int)(0.22f * RATE);
    for (int i = 0; i < len; i++) {
        float t = (float)i / RATE;
        ph += (48.0f + 110.0f * expf(-t * 30.0f)) / RATE;
        buf[(start + i) % n] += sinf(TAU_F * ph) * expf(-t * 14.0f) * vol;
    }
}
static void add_snare(float *buf, int n, int start, float vol) {
    int len = (int)(0.2f * RATE);
    float lp = 0;
    for (int i = 0; i < len; i++) {
        float t = (float)i / RATE;
        lp += (noisef() - lp) * 0.8f;
        buf[(start + i) % n] += (lp * expf(-t * 22.0f) * 0.8f + sinf(TAU_F * 190.0f * t) * expf(-t * 30.0f) * 0.4f) * vol;
    }
}
static void add_hat(float *buf, int n, int start, float vol) {
    int len = (int)(0.05f * RATE);
    float prev = 0;
    for (int i = 0; i < len; i++) {
        float t = (float)i / RATE, nz = noisef();
        buf[(start + i) % n] += (nz - prev) * expf(-t * 90.0f) * vol * 0.5f;
        prev = nz;
    }
}

/* A-minor march: Am Am F G Am Am F E, twice */
static const int PROG_ROOT[8] = { 45, 45, 41, 43, 45, 45, 41, 40 };
static const int PROG_THIRD[8] = { 48, 48, 45, 47, 48, 48, 45, 44 };
static const int PROG_FIFTH[8] = { 52, 52, 48, 50, 52, 52, 48, 47 };
static const int MEL_A[64] = {
    69, 0, 72, 0, 76, 0, 72, 69, 71, 0, 69, 0, 67, 0, 64, 0,
    65, 0, 69, 0, 72, 0, 69, 65, 67, 0, 71, 0, 74, 0, 71, 67,
    69, 0, 72, 0, 76, 0, 79, 76, 77, 76, 72, 0, 76, 0, 0, 0,
    77, 0, 76, 72, 69, 0, 72, 0, 71, 0, 68, 0, 64, 0, 0, 0 };
static const int MEL_B[64] = {
    76, 0, 76, 72, 69, 0, 72, 76, 81, 0, 79, 76, 72, 0, 76, 0,
    77, 0, 77, 72, 69, 0, 72, 77, 79, 0, 79, 74, 71, 0, 74, 79,
    81, 79, 76, 72, 69, 72, 76, 79, 81, 0, 0, 0, 76, 0, 72, 0,
    77, 76, 74, 72, 69, 72, 74, 76, 71, 68, 71, 76, 0, 0, 0, 0 };

static void render_track(int which) {
    int game = which == 1;
    float bpm = game ? 140.0f : 74.0f;
    int bars = game ? 16 : 8;
    int step = (int)(RATE * 60.0f / bpm / 4.0f); /* 16th note */
    int n = step * 16 * bars;
    float *b = calloc((size_t)n, sizeof(float));
    for (int bar = 0; bar < bars; bar++) {
        int ci = bar % 8, base = bar * 16 * step;
        const int *mel = (game && bar >= 8) ? MEL_B : MEL_A;
        float root = (float)PROG_ROOT[ci];
        if (game) {
            for (int s = 0; s < 16; s += 2) {
                float m = root;
                if (s == 4 || s == 12) m += 12;
                if (s == 14) m += 7;
                add_note(b, n, base + s * step, step * 1.6f / RATE, midi_hz(m), INST_BASS, 0.5f);
            }
            for (int s = 0; s < 16; s++) {
                int tone = (s % 4 == 3) ? 1 : s % 3;
                int mm = tone == 0 ? PROG_ROOT[ci] : (tone == 1 ? PROG_THIRD[ci] : PROG_FIFTH[ci]);
                add_note(b, n, base + s * step, step * 0.8f / RATE, midi_hz((float)mm + 24 + (s & 4 ? 12 : 0)), INST_ARP, 0.07f);
            }
            for (int s = 0; s < 16; s += 4) add_kick(b, n, base + s * step, 0.75f);
            add_snare(b, n, base + 4 * step, 0.55f);
            add_snare(b, n, base + 12 * step, 0.55f);
            if (bar % 8 == 7)
                for (int s = 12; s < 16; s++) add_snare(b, n, base + s * step, 0.35f);
            for (int s = 0; s < 16; s += 2) add_hat(b, n, base + s * step, 0.35f);
            for (int e = 0; e < 8; e++) {
                int m = mel[(bar % 8) * 8 + e];
                if (m) add_note(b, n, base + e * 2 * step, step * 1.7f / RATE, midi_hz((float)m), INST_LEAD, 0.30f);
            }
        } else {
            /* slow, ominous title theme */
            add_note(b, n, base, step * 15.5f / RATE, midi_hz(root - 12), INST_BASS, 0.55f);
            add_note(b, n, base, step * 16.0f / RATE, midi_hz((float)PROG_THIRD[ci]), INST_PAD, 0.16f);
            add_note(b, n, base, step * 16.0f / RATE, midi_hz((float)PROG_FIFTH[ci]), INST_PAD, 0.16f);
            add_note(b, n, base, step * 16.0f / RATE, midi_hz(root + 12), INST_PAD, 0.12f);
            add_kick(b, n, base, 0.7f);
            add_kick(b, n, base + 8 * step, 0.45f);
            add_snare(b, n, base + 12 * step, 0.25f);
            for (int e = 0; e < 8; e++) {
                int m = mel[(bar % 8) * 8 + e];
                if (m) add_note(b, n, base + e * 2 * step, step * 3.0f / RATE, midi_hz((float)m - 12), INST_LEAD, 0.28f);
            }
        }
    }
    /* normalise + convert */
    float peak = 0.01f;
    for (int i = 0; i < n; i++) if (fabsf(b[i]) > peak) peak = fabsf(b[i]);
    music[which] = malloc(sizeof(int16_t) * (size_t)n);
    music_n[which] = n;
    for (int i = 0; i < n; i++) music[which][i] = (int16_t)(clampf(b[i] / peak * 0.9f, -1, 1) * 30000.0f);
    free(b);
}

static void audio_lock(void) { if (adev) SDL_LockAudioDevice(adev); }
static void audio_unlock(void) { if (adev) SDL_UnlockAudioDevice(adev); }

static void play_sfx(int id, float vol, float pan) { /* pan -1..1 */
    if (vol < 0.02f) return;
    audio_lock();
    int slot = -1;
    for (int i = 0; i < NVOICE; i++) if (!voices[i].s) { slot = i; break; }
    if (slot < 0) slot = 0;
    float l = 1.0f - fmaxf(0.0f, pan) * 0.8f, r = 1.0f + fminf(0.0f, pan) * 0.8f;
    voices[slot] = (Voice){ &snd[id], 0, vol * sfx_vol * l, vol * sfx_vol * r };
    audio_unlock();
}
static void music_play(int track) {
    audio_lock();
    if (track != mus_cur) { mus_cur = track; mus_pos = 0; }
    audio_unlock();
}

static void mix_audio(int16_t *out, int frames) {
    for (int i = 0; i < frames; i++) {
        float l = 0, r = 0;
        if (music_on && mus_cur >= 0 && music[mus_cur]) {
            float m = music[mus_cur][mus_pos] * (1.0f / 32768.0f) * mus_vol;
            l += m;
            r += m;
            if (++mus_pos >= music_n[mus_cur]) mus_pos = 0;
        }
        for (int v = 0; v < NVOICE; v++) {
            if (!voices[v].s) continue;
            float s = voices[v].s->d[voices[v].pos] * (1.0f / 32768.0f);
            l += s * voices[v].vl;
            r += s * voices[v].vr;
            if (++voices[v].pos >= voices[v].s->n) voices[v].s = NULL;
        }
        /* soft clip */
        l = l / (1.0f + fabsf(l) * 0.35f);
        r = r / (1.0f + fabsf(r) * 0.35f);
        out[i * 2] = (int16_t)(clampf(l, -1, 1) * 32000.0f);
        out[i * 2 + 1] = (int16_t)(clampf(r, -1, 1) * 32000.0f);
    }
}
static void SDLCALL audio_cb(void *ud, Uint8 *stream, int len) {
    (void)ud;
    mix_audio((int16_t *)stream, len / 4);
}

/* ------------------------------------------------------------------ */
/* world                                                              */
/* ------------------------------------------------------------------ */
static uint8_t map[MAXMAP][MAXMAP];
static uint8_t pillar[MAXMAP][MAXMAP];
static uint8_t seen[MAXMAP][MAXMAP];
static float dopen[MAXMAP][MAXMAP];
static uint8_t dstate[MAXMAP][MAXMAP]; /* 0 closed 1 opening 2 open 3 closing */
static float dtimer[MAXMAP][MAXMAP];
static int MW, MH;

enum { I_HEALTH, I_AMMO, I_TREAS, I_GUN, I_PILLAR };
typedef struct { float x, y; int kind, spr, active; } Item;
static Item items[MAXI];
static int nitems;

enum { E_GUARD, E_SS, E_BOSS };
enum { ES_SLEEP, ES_CHASE, ES_ATTACK, ES_PAIN, ES_DEAD };
typedef struct { float x, y, timer, cool, animt; int type, state, hp, fired; } Enemy;
static Enemy en[MAXE];
static int nen;

static int level = 1;
static int total_enemies, total_treasure, got_kills, got_treasure;
static int exit_x, exit_y, exit_ax, exit_ay; /* exit wall tile + adjacent floor tile */
static int start_x, start_y;
static float start_ang;
static short fieldP[MAXMAP][MAXMAP], fieldE[MAXMAP][MAXMAP];

static int in_map(int x, int y) { return x >= 0 && y >= 0 && x < MW && y < MH; }
static int passable(int x, int y) {
    return in_map(x, y) && (map[y][x] == T_EMPTY || map[y][x] == T_DOOR) && !pillar[y][x];
}
static void bfs(short f[MAXMAP][MAXMAP], int sx, int sy) {
    static int qx[MAXMAP * MAXMAP], qy[MAXMAP * MAXMAP];
    for (int y = 0; y < MAXMAP; y++)
        for (int x = 0; x < MAXMAP; x++) f[y][x] = -1;
    if (!passable(sx, sy)) return;
    int h = 0, t = 0;
    qx[t] = sx; qy[t++] = sy; f[sy][sx] = 0;
    static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
    while (h < t) {
        int x = qx[h], y = qy[h++];
        for (int d = 0; d < 4; d++) {
            int nx = x + dx[d], ny = y + dy[d];
            if (passable(nx, ny) && f[ny][nx] < 0) {
                f[ny][nx] = (short)(f[y][x] + 1);
                qx[t] = nx; qy[t++] = ny;
            }
        }
    }
}

typedef struct { int cx, cy, hw, hh, wall, link[4], depth; } Room; /* link: 0=E 1=W 2=S 3=N */
#define CELL 11

static void add_item(float x, float y, int kind) {
    if (nitems >= MAXI) return;
    static const int spr[5] = { SP_HEALTH, SP_AMMO, SP_TREAS1, SP_GUN, SP_PILLAR };
    int s = spr[kind];
    if (kind == I_TREAS && irnd(3) == 0) s = SP_TREAS2;
    items[nitems++] = (Item){ x, y, kind, s, 1 };
    if (kind == I_TREAS) total_treasure++;
}
static void add_enemy(float x, float y, int type) {
    if (nen >= MAXE) return;
    static const int hp[3] = { 25, 50, 220 };
    en[nen++] = (Enemy){ x, y, 0, 0, frnd() * 2, type, ES_SLEEP, hp[type], 0 };
    total_enemies++;
}

static void gen_level(int L) {
    int gx = L == 1 ? 3 : (L == 2 ? 4 : (L == 3 ? 4 : 5)), gy = L == 1 ? 3 : (L == 2 ? 3 : (L == 3 ? 4 : 4));
    uint32_t save = rng_s;
    rng_s = 0x9E3779B9u * (uint32_t)(L * 7 + 3) + 17;
    for (int i = 0; i < 8; i++) rnd();
    memset(map, 0, sizeof map);
    memset(pillar, 0, sizeof pillar);
    memset(seen, 0, sizeof seen);
    memset(dopen, 0, sizeof dopen);
    memset(dstate, 0, sizeof dstate);
    nitems = nen = 0;
    total_enemies = total_treasure = got_kills = got_treasure = 0;
    MW = gx * CELL;
    MH = gy * CELL;
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) map[y][x] = T_STONE;

    static Room rooms[64];
    int nr = gx * gy;
    for (int i = 0; i < nr; i++) {
        Room *r = &rooms[i];
        r->cx = (i % gx) * CELL + 5;
        r->cy = (i / gx) * CELL + 5;
        r->hw = 2 + irnd(2);
        r->hh = 2 + irnd(2);
        r->wall = 1 + irnd(4);
        r->depth = -1;
        for (int k = 0; k < 4; k++) r->link[k] = -1;
        for (int y = r->cy - r->hh - 1; y <= r->cy + r->hh + 1; y++)
            for (int x = r->cx - r->hw - 1; x <= r->cx + r->hw + 1; x++) {
                int ring = (x == r->cx - r->hw - 1 || x == r->cx + r->hw + 1 || y == r->cy - r->hh - 1 || y == r->cy + r->hh + 1);
                map[y][x] = ring ? (uint8_t)r->wall : T_EMPTY;
            }
    }
    /* spanning tree by randomised DFS + a few extra links */
    static int stack[64], visited[64];
    memset(visited, 0, sizeof visited);
    int sp = 0;
    stack[sp++] = 0;
    visited[0] = 1;
#define CONNECT(a, b, da, db) do { rooms[a].link[da] = (b); rooms[b].link[db] = (a); } while (0)
    while (sp) {
        int c = stack[sp - 1], cand[4], nc = 0;
        int cx = c % gx, cy = c / gx;
        if (cx + 1 < gx && !visited[c + 1]) cand[nc++] = 0;
        if (cx > 0 && !visited[c - 1]) cand[nc++] = 1;
        if (cy + 1 < gy && !visited[c + gx]) cand[nc++] = 2;
        if (cy > 0 && !visited[c - gx]) cand[nc++] = 3;
        if (!nc) { sp--; continue; }
        int d = cand[irnd(nc)];
        int o = d == 0 ? c + 1 : (d == 1 ? c - 1 : (d == 2 ? c + gx : c - gx));
        CONNECT(c, o, d, d ^ 1);
        visited[o] = 1;
        stack[sp++] = o;
    }
    for (int i = 0; i < nr; i++) {
        int cx = i % gx, cy = i / gx;
        if (cx + 1 < gx && rooms[i].link[0] < 0 && irnd(100) < 30) CONNECT(i, i + 1, 0, 1);
        if (cy + 1 < gy && rooms[i].link[2] < 0 && irnd(100) < 30) CONNECT(i, i + gx, 2, 3);
    }
    /* carve corridors; ring tiles become doors */
    for (int i = 0; i < nr; i++) {
        Room *a = &rooms[i];
        if (a->link[0] >= 0) {
            Room *b = &rooms[a->link[0]];
            for (int x = a->cx + a->hw + 1; x <= b->cx - b->hw - 1; x++) map[a->cy][x] = T_EMPTY;
            map[a->cy][a->cx + a->hw + 1] = T_DOOR;
            map[a->cy][b->cx - b->hw - 1] = T_DOOR;
        }
        if (a->link[2] >= 0) {
            Room *b = &rooms[a->link[2]];
            for (int y = a->cy + a->hh + 1; y <= b->cy - b->hh - 1; y++) map[y][a->cx] = T_EMPTY;
            map[a->cy + a->hh + 1][a->cx] = T_DOOR;
            map[b->cy - b->hh - 1][a->cx] = T_DOOR;
        }
    }
    /* depth from room 0 */
    int q[64], qh = 0, qt = 0;
    q[qt++] = 0;
    rooms[0].depth = 0;
    while (qh < qt) {
        int c = q[qh++];
        for (int k = 0; k < 4; k++) {
            int o = rooms[c].link[k];
            if (o >= 0 && rooms[o].depth < 0) { rooms[o].depth = rooms[c].depth + 1; q[qt++] = o; }
        }
    }
    /* exit room = deepest room that still has a free wall side */
    int exit_room = -1, exit_side = -1;
    for (int pass = 0; pass < nr && exit_room < 0; pass++) {
        int best = -1;
        for (int i = 1; i < nr; i++)
            if (rooms[i].depth >= 0 && (best < 0 || rooms[i].depth > rooms[best].depth) &&
                !(i == exit_room)) {
                int skip = 0;
                (void)skip;
                int free_side = 0;
                for (int k = 0; k < 4; k++) if (rooms[i].link[k] < 0) free_side = 1;
                if (free_side) best = i;
            }
        exit_room = best;
    }
    if (exit_room < 0) exit_room = nr - 1;
    {
        Room *r = &rooms[exit_room];
        int opts[4], no = 0;
        for (int k = 0; k < 4; k++) if (r->link[k] < 0) opts[no++] = k;
        exit_side = no ? opts[irnd(no)] : 0;
        int ex = r->cx, ey = r->cy;
        if (exit_side == 0) { ex = r->cx + r->hw + 1; exit_ax = ex - 1; exit_ay = ey; }
        else if (exit_side == 1) { ex = r->cx - r->hw - 1; exit_ax = ex + 1; exit_ay = ey; }
        else if (exit_side == 2) { ey = r->cy + r->hh + 1; exit_ax = ex; exit_ay = ey - 1; }
        else { ey = r->cy - r->hh - 1; exit_ax = ex; exit_ay = ey + 1; }
        map[ey][ex] = T_EXIT;
        exit_x = ex;
        exit_y = ey;
    }
    /* pillars in some room corners */
    for (int i = 0; i < nr; i++) {
        Room *r = &rooms[i];
        if (i == 0 || irnd(100) >= 55) continue;
        int xs[2] = { r->cx - r->hw, r->cx + r->hw }, ys[2] = { r->cy - r->hh, r->cy + r->hh };
        for (int a = 0; a < 2; a++)
            for (int b = 0; b < 2; b++) {
                pillar[ys[b]][xs[a]] = 1;
                add_item(xs[a] + 0.5f, ys[b] + 0.5f, I_PILLAR);
            }
    }
    /* player start */
    start_x = rooms[0].cx;
    start_y = rooms[0].cy;
    start_ang = rooms[0].link[0] >= 0 ? 0.0f : (rooms[0].link[2] >= 0 ? PI_F / 2 : 0.0f);
    /* populate */
    int gun_room = nr > 3 ? 2 + irnd(nr - 3) : 1;
    if (L < 2) gun_room = -1;
    for (int i = 0; i < nr; i++) {
        Room *r = &rooms[i];
        int tries;
#define RANDPOS(px, py) do { tries = 0; do { px = r->cx - r->hw + irnd(2 * r->hw + 1); py = r->cy - r->hh + irnd(2 * r->hh + 1); tries++; } while (pillar[py][px] && tries < 50); } while (0)
        if (i != 0) {
            int cnt = 1 + irnd(2) + (L >= 3 ? irnd(2) : 0) + (L >= 5 ? 1 : 0);
            for (int k = 0; k < cnt; k++) {
                int px, py;
                RANDPOS(px, py);
                int ss_chance = L == 1 ? 0 : (L == 2 ? 20 : (L == 3 ? 35 : 50));
                add_enemy(px + 0.5f, py + 0.5f, irnd(100) < ss_chance ? E_SS : E_GUARD);
            }
        }
        if (L == NLEVELS && i == exit_room) {
            add_enemy(r->cx + 0.5f, r->cy + 0.5f, E_BOSS);
        }
        int nit = (i == 0) ? 1 : irnd(3);
        for (int k = 0; k < nit; k++) {
            int px, py, roll = irnd(100);
            RANDPOS(px, py);
            int kind = roll < 40 ? I_TREAS : (roll < 70 ? I_AMMO : I_HEALTH);
            if (i == 0) kind = I_AMMO;
            add_item(px + 0.5f, py + 0.5f, kind);
        }
        if (i == gun_room) {
            int px, py;
            RANDPOS(px, py);
            add_item(px + 0.5f, py + 0.5f, I_GUN);
        }
    }
    /* doors init */
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            dopen[y][x] = 0;
            dstate[y][x] = 0;
        }
    rng_s = save;
}

/* ------------------------------------------------------------------ */
/* player + game state                                                */
/* ------------------------------------------------------------------ */
enum { S_TITLE, S_PLAY, S_DYING, S_LEVELDONE, S_GAMEOVER, S_WIN, S_PAUSE };
typedef struct {
    float x, y, ang, dirx, diry, planex, planey;
    int health, ammo, weapon, has_mg, lives, score, next_life;
    float wcool, wfire, bob, hurt, flash, noise, face_t;
} Player;
static Player P;
static int gstate = S_TITLE;
static float state_t, level_time, game_time;
static int godmode = 0, automap = 0;
static uint32_t bonus_text_kills, bonus_text_treas;
static float pain_dir;

typedef struct {
    float fwd, strafe, turn; /* -1..1 ; turn in rad this frame too */
    float mouse_turn;
    int fire, use, run;
    int select;   /* 0 none, 1..3 */
    int enter, back;
} Input;

static void set_dir(void) {
    P.dirx = cosf(P.ang);
    P.diry = sinf(P.ang);
    P.planex = -P.diry * 0.66f;
    P.planey = P.dirx * 0.66f;
}

static void snd_at(int id, float x, float y, float vol) {
    float dx = x - P.x, dy = y - P.y, d = sqrtf(dx * dx + dy * dy);
    float pan = d > 0.2f ? (dx * (-P.diry) + dy * P.dirx) / d : 0.0f;
    play_sfx(id, vol / (1.0f + 0.12f * d), pan);
}

static int solid_cell(int x, int y) {
    if (!in_map(x, y)) return 1;
    int t = map[y][x];
    if (t == T_EMPTY) return pillar[y][x];
    if (t == T_DOOR) return dopen[y][x] < 0.85f;
    return 1;
}
static int free_at(float x, float y, float r) {
    return !solid_cell((int)floorf(x - r), (int)floorf(y - r)) && !solid_cell((int)floorf(x + r), (int)floorf(y - r)) &&
           !solid_cell((int)floorf(x - r), (int)floorf(y + r)) && !solid_cell((int)floorf(x + r), (int)floorf(y + r));
}
static int los(float x0, float y0, float x1, float y1) {
    float dx = x1 - x0, dy = y1 - y0, d = sqrtf(dx * dx + dy * dy);
    int n = (int)(d / 0.12f) + 1;
    for (int i = 1; i < n; i++) {
        float x = x0 + dx * i / n, y = y0 + dy * i / n;
        int cx = (int)x, cy = (int)y;
        if (!in_map(cx, cy)) return 0;
        int t = map[cy][cx];
        if (t != T_EMPTY && !(t == T_DOOR && dopen[cy][cx] > 0.7f)) return 0;
    }
    return 1;
}

static void open_door(int x, int y) {
    if (!in_map(x, y) || map[y][x] != T_DOOR) return;
    if (dstate[y][x] == 0 || dstate[y][x] == 3) {
        dstate[y][x] = 1;
        snd_at(SND_DOOR, x + 0.5f, y + 0.5f, 0.9f);
    } else if (dstate[y][x] == 2) dtimer[y][x] = 4.0f;
}

static int door_occupied(int x, int y) { /* anything whose body overlaps the door cell */
    const float m = 0.4f;
    if (P.x > x - m && P.x < x + 1 + m && P.y > y - m && P.y < y + 1 + m) return 1;
    for (int i = 0; i < nen; i++)
        if (en[i].state != ES_DEAD && en[i].x > x - m && en[i].x < x + 1 + m && en[i].y > y - m && en[i].y < y + 1 + m) return 1;
    return 0;
}
static void update_doors(float dt) {
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            if (map[y][x] != T_DOOR) continue;
            switch (dstate[y][x]) {
            case 1:
                dopen[y][x] += dt * 1.6f;
                if (dopen[y][x] >= 1.0f) { dopen[y][x] = 1.0f; dstate[y][x] = 2; dtimer[y][x] = 4.0f; }
                break;
            case 2:
                dtimer[y][x] -= dt;
                if (dtimer[y][x] <= 0) {
                    if (door_occupied(x, y)) dtimer[y][x] = 0.5f;
                    else { dstate[y][x] = 3; snd_at(SND_DOORC, x + 0.5f, y + 0.5f, 0.7f); }
                }
                break;
            case 3:
                if (door_occupied(x, y)) { dstate[y][x] = 1; break; }
                dopen[y][x] -= dt * 1.6f;
                if (dopen[y][x] <= 0) { dopen[y][x] = 0; dstate[y][x] = 0; }
                break;
            }
        }
}

static void start_level(int L) {
    level = L;
    gen_level(L);
    bfs(fieldE, exit_ax, exit_ay);
    P.x = start_x + 0.5f;
    P.y = start_y + 0.5f;
    P.ang = start_ang;
    set_dir();
    P.health = 100;
    P.wcool = P.wfire = P.hurt = P.flash = P.noise = 0;
    if (P.ammo < 8) P.ammo = 8;
    if (P.weapon == W_MG && !P.has_mg) P.weapon = W_PISTOL;
    level_time = 0;
    bfs(fieldP, (int)P.x, (int)P.y);
}
static void new_game(void) {
    memset(&P, 0, sizeof P);
    P.lives = 3;
    P.ammo = 8;
    P.weapon = W_PISTOL;
    P.next_life = 20000;
    start_level(1);
    gstate = S_PLAY;
    state_t = 0;
    music_play(1);
}

static void add_score(int s) {
    P.score += s;
    if (P.score >= P.next_life) {
        P.next_life += 20000;
        if (P.lives < 9) P.lives++;
        P.health = 100;
        P.flash = 0.6f;
        play_sfx(SND_EXIT, 0.8f, 0);
    }
}

static void hurt_player(int dmg, float fx, float fy) {
    if (godmode || gstate != S_PLAY) return;
    P.health -= dmg;
    P.hurt = 0.5f;
    pain_dir = atan2f(fy - P.y, fx - P.x);
    play_sfx(SND_HURT, 1.0f, 0);
    if (P.health <= 0) {
        P.health = 0;
        gstate = S_DYING;
        state_t = 0;
        play_sfx(SND_GAMEOVER, 0.8f, 0);
    }
}

static void hurt_enemy(Enemy *e, int dmg) {
    if (e->state == ES_DEAD) return;
    e->hp -= dmg;
    if (e->hp <= 0) {
        e->state = ES_DEAD;
        e->timer = 0;
        got_kills++;
        static const int pts[3] = { 100, 500, 5000 };
        add_score(pts[e->type]);
        snd_at(e->type == E_BOSS ? SND_BOSSDIE : SND_DEATH, e->x, e->y, 1.0f);
        if (e->type != E_BOSS && nitems < MAXI) {
            items[nitems++] = (Item){ e->x, e->y, I_AMMO, SP_AMMO, 1 };
        }
    } else {
        if (e->state == ES_SLEEP) snd_at(SND_ALERT, e->x, e->y, 0.8f);
        if (e->type != E_BOSS && irnd(100) < 55) {
            e->state = ES_PAIN;
            e->timer = 0.3f;
            snd_at(SND_PAIN, e->x, e->y, 0.8f);
        } else if (e->state == ES_SLEEP) e->state = ES_CHASE;
    }
}

static void player_fire(void) {
    if (P.wcool > 0) return;
    if (P.weapon != W_KNIFE) {
        if (P.ammo <= 0) {
            play_sfx(SND_EMPTY, 0.8f, 0);
            P.wcool = 0.3f;
            P.weapon = W_KNIFE;
            return;
        }
        P.ammo--;
    }
    static const float cool[3] = { 0.38f, 0.42f, 0.11f };
    P.wcool = cool[P.weapon];
    P.wfire = P.weapon == W_MG ? 0.06f : 0.14f;
    P.noise = 1.5f;
    play_sfx(P.weapon == W_KNIFE ? SND_KNIFE : (P.weapon == W_PISTOL ? SND_PISTOL : SND_MG), 1.0f, 0);
    float range = P.weapon == W_KNIFE ? 1.5f : 99.0f;
    float rad = P.weapon == W_KNIFE ? 0.55f : 0.34f;
    float jitter = P.weapon == W_MG ? (frnd() - 0.5f) * 0.05f : 0.0f;
    Enemy *best = NULL;
    float bd = 1e9f;
    for (int i = 0; i < nen; i++) {
        Enemy *e = &en[i];
        if (e->state == ES_DEAD) continue;
        float rx = e->x - P.x, ry = e->y - P.y;
        float depth = rx * P.dirx + ry * P.diry;
        float lat = rx * (-P.diry) + ry * P.dirx - depth * jitter;
        if (depth > 0.1f && depth < range && fabsf(lat) < rad && depth < bd && los(P.x, P.y, e->x, e->y)) {
            best = e;
            bd = depth;
        }
    }
    if (best) {
        int dmg = P.weapon == W_KNIFE ? 10 + irnd(11) : (P.weapon == W_PISTOL ? 14 + irnd(17) : 11 + irnd(13));
        hurt_enemy(best, dmg);
    }
}

static void try_use(void) {
    for (float d = 0.6f; d <= 1.6f; d += 0.25f) {
        int cx = (int)(P.x + P.dirx * d), cy = (int)(P.y + P.diry * d);
        if (!in_map(cx, cy)) return;
        if (map[cy][cx] == T_DOOR) { open_door(cx, cy); return; }
        if (map[cy][cx] == T_EXIT) {
            if (gstate == S_PLAY) {
                gstate = S_LEVELDONE;
                state_t = 0;
                play_sfx(SND_EXIT, 1.0f, 0);
                int kp = total_enemies ? got_kills * 100 / total_enemies : 100;
                int tp = total_treasure ? got_treasure * 100 / total_treasure : 100;
                bonus_text_kills = (uint32_t)kp;
                bonus_text_treas = (uint32_t)tp;
                add_score(1000 + kp * 10 + tp * 10 + (kp == 100 ? 2000 : 0));
            }
            return;
        }
        if (map[cy][cx] != T_EMPTY) return;
    }
}

static void update_player(float dt, const Input *in) {
    float spd = (in->run ? 5.0f : 3.2f);
    float ts = (in->run ? 3.0f : 2.3f);
    P.ang += in->turn * ts * dt + in->mouse_turn;
    set_dir();
    float mx = P.dirx * in->fwd + (-P.diry) * in->strafe;
    float my = P.diry * in->fwd + P.dirx * in->strafe;
    float ml = sqrtf(mx * mx + my * my);
    if (ml > 1) { mx /= ml; my /= ml; ml = 1; }
    float nx = P.x + mx * spd * dt, ny = P.y + my * spd * dt;
    const float R = 0.28f;
    if (free_at(nx, P.y, R)) P.x = nx;
    if (free_at(P.x, ny, R)) P.y = ny;
    /* keep out of living enemies */
    for (int i = 0; i < nen; i++) {
        if (en[i].state == ES_DEAD) continue;
        float dx = P.x - en[i].x, dy = P.y - en[i].y, d = sqrtf(dx * dx + dy * dy);
        if (d < 0.5f && d > 0.001f) {
            float push = (0.5f - d);
            float px = P.x + dx / d * push, py = P.y + dy / d * push;
            if (free_at(px, P.y, R)) P.x = px;
            if (free_at(P.x, py, R)) P.y = py;
        }
    }
    if (ml > 0.05f) P.bob += dt * (in->run ? 11.0f : 8.0f);
    P.wcool -= dt;
    P.wfire -= dt;
    P.hurt -= dt;
    P.flash -= dt;
    P.noise -= dt;
    if (in->select) {
        int w = in->select - 1;
        if (w == W_MG && !P.has_mg) w = P.weapon;
        if (w != P.weapon) { P.weapon = w; P.wcool = 0.15f; play_sfx(SND_MENU, 0.3f, 0); }
    }
    if (in->fire) player_fire();
    if (in->use) try_use();
    /* pickups */
    for (int i = 0; i < nitems; i++) {
        Item *it = &items[i];
        if (!it->active || it->kind == I_PILLAR) continue;
        float dx = it->x - P.x, dy = it->y - P.y;
        if (dx * dx + dy * dy > 0.45f * 0.45f) continue;
        switch (it->kind) {
        case I_HEALTH:
            if (P.health >= 100) continue;
            P.health = P.health + 25 > 100 ? 100 : P.health + 25;
            play_sfx(SND_HEALTH, 0.9f, 0);
            break;
        case I_AMMO:
            if (P.ammo >= 99) continue;
            if (P.ammo == 0 && P.weapon == W_KNIFE) P.weapon = P.has_mg ? W_MG : W_PISTOL;
            P.ammo = P.ammo + 8 > 99 ? 99 : P.ammo + 8;
            play_sfx(SND_AMMO, 0.9f, 0);
            break;
        case I_TREAS:
            got_treasure++;
            add_score(it->spr == SP_TREAS2 ? 500 : 100);
            play_sfx(SND_TREASURE, 0.9f, 0);
            break;
        case I_GUN:
            P.has_mg = 1;
            P.weapon = W_MG;
            P.ammo = P.ammo + 20 > 99 ? 99 : P.ammo + 20;
            play_sfx(SND_GUNGET, 1.0f, 0);
            break;
        }
        it->active = 0;
        P.flash = 0.25f;
    }
}

/* ----- enemies ----- */
static void enemy_try_move(Enemy *e, float tx, float ty, float speed, float dt) {
    float dx = tx - e->x, dy = ty - e->y, d = sqrtf(dx * dx + dy * dy);
    if (d < 0.001f) return;
    float step = speed * dt;
    if (step > d) step = d;
    float nx = e->x + dx / d * step, ny = e->y + dy / d * step;
    const float R = 0.25f;
    /* open doors we bump into */
    int cx = (int)(e->x + dx / d * 0.6f), cy = (int)(e->y + dy / d * 0.6f);
    if (in_map(cx, cy) && map[cy][cx] == T_DOOR && dopen[cy][cx] < 0.85f) open_door(cx, cy);
    if (free_at(nx, e->y, R)) e->x = nx;
    if (free_at(e->x, ny, R)) e->y = ny;
}

static void enemy_shoot(Enemy *e, float dist) {
    snd_at(SND_ESHOT, e->x, e->y, 1.0f);
    float p = dist < 2.0f ? 0.95f : clampf(0.9f - dist * 0.07f, 0.2f, 0.9f);
    if (frnd() < p) {
        static const int lo[3] = { 4, 7, 9 }, sp[3] = { 8, 9, 11 };
        int dmg = lo[e->type] + irnd(sp[e->type]);
        if (dist > 6) dmg = dmg * 2 / 3 + 1;
        hurt_player(dmg, e->x, e->y);
    }
}

static void update_enemies(float dt) {
    static const float spd[3] = { 1.5f, 2.1f, 1.9f };
    int ptx = (int)P.x, pty = (int)P.y;
    static int last_tx = -1, last_ty = -1;
    if (ptx != last_tx || pty != last_ty || level_time < 0.05f) {
        bfs(fieldP, ptx, pty);
        last_tx = ptx;
        last_ty = pty;
    }
    for (int i = 0; i < nen; i++) {
        Enemy *e = &en[i];
        if (e->state == ES_DEAD) continue;
        float dx = P.x - e->x, dy = P.y - e->y, d = sqrtf(dx * dx + dy * dy);
        e->animt += dt;
        e->cool -= dt;
        int see = d < 16.0f && los(e->x, e->y, P.x, P.y);
        switch (e->state) {
        case ES_SLEEP:
            if ((see && (d < 3.0f || frnd() < dt * 1.2f * (1.0f - d / 16.0f) * 3.0f)) ||
                (P.noise > 0 && d < 11.0f && (see || fieldP[(int)e->y][(int)e->x] >= 0))) {
                if (!see && fieldP[(int)e->y][(int)e->x] > 14) break;
                e->state = ES_CHASE;
                e->cool = 0.3f + frnd() * 0.4f;
                snd_at(SND_ALERT, e->x, e->y, 0.9f);
            }
            break;
        case ES_CHASE:
            if (see && d < (e->type == E_GUARD ? 7.0f : 9.0f) && e->cool <= 0) {
                e->state = ES_ATTACK;
                e->timer = e->type == E_BOSS ? 0.9f : 0.6f;
                e->fired = 0;
                break;
            }
            if (see && d < 1.2f) break; /* don't hug the player */
            if (see && d < 4.5f) enemy_try_move(e, P.x, P.y, spd[e->type], dt);
            else {
                int tx = (int)e->x, ty = (int)e->y;
                if (!in_map(tx, ty) || fieldP[ty][tx] < 0) { enemy_try_move(e, P.x, P.y, spd[e->type], dt); break; }
                int bx = tx, by = ty, bd = fieldP[ty][tx];
                static const int ddx[4] = { 1, -1, 0, 0 }, ddy[4] = { 0, 0, 1, -1 };
                for (int k = 0; k < 4; k++) {
                    int nx = tx + ddx[k], ny = ty + ddy[k];
                    if (in_map(nx, ny) && fieldP[ny][nx] >= 0 && fieldP[ny][nx] < bd) { bd = fieldP[ny][nx]; bx = nx; by = ny; }
                }
                if (bx == tx && by == ty) enemy_try_move(e, P.x, P.y, spd[e->type], dt);
                else enemy_try_move(e, bx + 0.5f, by + 0.5f, spd[e->type], dt);
            }
            break;
        case ES_ATTACK:
            e->timer -= dt;
            if (!e->fired && e->timer < (e->type == E_BOSS ? 0.6f : 0.3f)) {
                e->fired = 1;
                enemy_shoot(e, d);
            }
            if (e->type == E_BOSS && e->fired == 1 && e->timer < 0.3f) {
                e->fired = 2;
                enemy_shoot(e, d);
            }
            if (e->timer <= 0) {
                e->state = ES_CHASE;
                e->cool = (e->type == E_GUARD ? 0.5f : 0.3f) + frnd() * 0.7f;
            }
            break;
        case ES_PAIN:
            e->timer -= dt;
            if (e->timer <= 0) { e->state = ES_CHASE; e->cool = 0.2f; }
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* rendering                                                          */
/* ------------------------------------------------------------------ */
static float zbuf[SCR_W];

static inline uint32_t shade_px(uint32_t c, int sh) { /* sh 0..256 */
    uint32_t r = (((c >> 16) & 255) * sh) >> 8, g = (((c >> 8) & 255) * sh) >> 8, b = ((c & 255) * sh) >> 8;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

typedef struct { float x, y, ang, dirx, diry, planex, planey; } Cam;

static void render_flats(void) {
    int half = VIEW_H / 2;
    for (int y = 0; y < VIEW_H; y++) {
        uint32_t c;
        if (y < half) {
            float f = 1.0f - (float)y / half; /* 1 at top, 0 at horizon */
            c = RGB(46 - (int)(14 * f), 48 - (int)(14 * f), 60 - (int)(16 * f));
            c = RGB(28 + (int)(22 * (1 - f)), 30 + (int)(22 * (1 - f)), 40 + (int)(26 * (1 - f)));
        } else {
            float f = (float)(y - half) / half; /* 0 at horizon, 1 at bottom */
            c = RGB(34 + (int)(62 * f), 32 + (int)(58 * f), 30 + (int)(54 * f));
        }
        uint32_t *row = &fb[y * SCR_W];
        for (int x = 0; x < SCR_W; x++) row[x] = c;
    }
}

static void render_walls(const Cam *c) {
    for (int x = 0; x < SCR_W; x++) {
        float camx = 2.0f * x / SCR_W - 1.0f;
        float rdx = c->dirx + c->planex * camx, rdy = c->diry + c->planey * camx;
        int mx = (int)c->x, my = (int)c->y;
        float ddx = rdx == 0 ? 1e30f : fabsf(1.0f / rdx), ddy = rdy == 0 ? 1e30f : fabsf(1.0f / rdy);
        int stx = rdx < 0 ? -1 : 1, sty = rdy < 0 ? -1 : 1;
        float sdx = rdx < 0 ? (c->x - mx) * ddx : (mx + 1.0f - c->x) * ddx;
        float sdy = rdy < 0 ? (c->y - my) * ddy : (my + 1.0f - c->y) * ddy;
        int side = 0, hit = 0;
        float pd = 0, wallx = 0;
        int tile = 0;
        for (int guard = 0; guard < 200 && !hit; guard++) {
            if (sdx < sdy) { sdx += ddx; mx += stx; side = 0; }
            else { sdy += ddy; my += sty; side = 1; }
            if (!in_map(mx, my)) break;
            seen[my][mx] = 1;
            tile = map[my][mx];
            if (tile == T_EMPTY) continue;
            float d = side == 0 ? sdx - ddx : sdy - ddy;
            float wx = side == 0 ? c->y + d * rdy : c->x + d * rdx;
            wx -= floorf(wx);
            if (tile == T_DOOR) {
                if (wx + dopen[my][mx] >= 1.0f) continue;
                wx += dopen[my][mx];
            }
            pd = d;
            wallx = wx;
            hit = 1;
        }
        zbuf[x] = hit ? pd : 1e9f;
        if (!hit) continue;
        if (pd < 0.01f) pd = 0.01f;
        int lh = (int)(VIEW_H / pd);
        int y0 = VIEW_H / 2 - lh / 2, y1 = VIEW_H / 2 + lh / 2;
        int tx = (int)(wallx * TEX);
        if (tx > TEX - 1) tx = TEX - 1;
        if (side == 0 && rdx > 0) tx = TEX - 1 - tx;
        if (side == 1 && rdy < 0) tx = TEX - 1 - tx;
        const uint32_t *col = &walltex[tile - 1][tx * TEX];
        float stp = (float)TEX / lh;
        float tpos = 0;
        if (y0 < 0) { tpos = -y0 * stp; y0 = 0; }
        if (y1 > VIEW_H) y1 = VIEW_H;
        float lite = clampf(1.15f - pd * 0.055f, 0.28f, 1.0f);
        if (side) lite *= 0.72f;
        int sh = (int)(lite * 256);
        for (int y = y0; y < y1; y++) {
            int ty = (int)tpos;
            if (ty > TEX - 1) ty = TEX - 1;
            fb[y * SCR_W + x] = shade_px(col[ty], sh);
            tpos += stp;
        }
    }
}

typedef struct { float dist; float x, y; int spr; } DrawSpr;
static DrawSpr dlist[MAXE + MAXI];

static int cmp_spr(const void *a, const void *b) {
    float da = ((const DrawSpr *)a)->dist, db = ((const DrawSpr *)b)->dist;
    return da < db ? 1 : (da > db ? -1 : 0);
}

static void render_sprites(const Cam *c, int show_enemies) {
    int n = 0;
    for (int i = 0; i < nitems; i++) {
        if (!items[i].active) continue;
        float dx = items[i].x - c->x, dy = items[i].y - c->y;
        dlist[n++] = (DrawSpr){ dx * dx + dy * dy, items[i].x, items[i].y, items[i].spr };
    }
    if (show_enemies)
        for (int i = 0; i < nen; i++) {
            Enemy *e = &en[i];
            int pose;
            switch (e->state) {
            case ES_DEAD: pose = 5; break;
            case ES_PAIN: pose = 4; break;
            case ES_ATTACK: pose = (e->fired || e->timer < 0.35f) ? 3 : 0; break;
            case ES_CHASE: pose = ((int)(e->animt * 4.5f) & 1) ? 1 : 2; break;
            default: pose = 0;
            }
            float dx = e->x - c->x, dy = e->y - c->y;
            dlist[n++] = (DrawSpr){ dx * dx + dy * dy + (e->state == ES_DEAD ? 0.5f : 0.0f), e->x, e->y, e->type * 6 + pose };
        }
    qsort(dlist, (size_t)n, sizeof(DrawSpr), cmp_spr);
    float invdet = 1.0f / (c->planex * c->diry - c->dirx * c->planey);
    for (int i = 0; i < n; i++) {
        float sx = dlist[i].x - c->x, sy = dlist[i].y - c->y;
        float tx = invdet * (c->diry * sx - c->dirx * sy);
        float ty = invdet * (-c->planey * sx + c->planex * sy);
        if (ty < 0.15f) continue;
        int scr_x = (int)((SCR_W / 2) * (1.0f + tx / ty));
        int sz = (int)fabsf(VIEW_H / ty);
        if (sz < 2) continue;
        int x0 = scr_x - sz / 2, x1 = scr_x + sz / 2;
        int y0 = VIEW_H / 2 - sz / 2, y1 = y0 + sz;
        const uint32_t *t = sprtex[dlist[i].spr];
        int sh = (int)(clampf(1.15f - ty * 0.055f, 0.3f, 1.0f) * 256);
        for (int x = x0 < 0 ? 0 : x0; x < x1 && x < SCR_W; x++) {
            if (ty >= zbuf[x]) continue;
            int tcx = (x - x0) * TEX / sz;
            if (tcx > TEX - 1) tcx = TEX - 1;
            const uint32_t *col = &t[tcx * TEX];
            int ys = y0 < 0 ? 0 : y0, ye = y1 > VIEW_H ? VIEW_H : y1;
            for (int y = ys; y < ye; y++) {
                int tcy = (y - y0) * TEX / sz;
                if (tcy > TEX - 1) tcy = TEX - 1;
                uint32_t px = col[tcy];
                if (px >> 24) fb[y * SCR_W + x] = shade_px(px, sh);
            }
        }
    }
}

static void render_weapon(void) {
    int w = P.weapon;
    int frame = P.wfire > 0 ? 1 : 0;
    const uint32_t *t = wpntex[w * 2 + frame];
    int sc = 4;
    float bobx = sinf(P.bob) * 9.0f, boby = fabsf(cosf(P.bob)) * 7.0f;
    float rec = P.wfire > 0 ? 10.0f : 0.0f;
    int ox = SCR_W / 2 - TEX * sc / 2 + (int)bobx + (w == W_KNIFE ? 50 : 0);
    int oy = VIEW_H - TEX * sc + 40 + (int)(boby + rec);
    for (int x = 0; x < TEX * sc; x++) {
        int sx = ox + x;
        if ((unsigned)sx >= SCR_W) continue;
        const uint32_t *col = &t[(x / sc) * TEX];
        for (int y = 0; y < TEX * sc; y++) {
            int sy = oy + y;
            if (sy < 0 || sy >= VIEW_H) continue;
            uint32_t px = col[y / sc];
            if (px >> 24) fb[sy * SCR_W + sx] = px;
        }
    }
}

static void render_world(const Cam *c) {
    render_flats();
    render_walls(c);
    render_sprites(c, 1);
}

static void draw_face(int x, int y) {
    uint32_t skin = RGB(235, 190, 150);
    frect(x, y, 48, 48, RGB(20, 24, 50));
    frect(x + 4, y + 2, 40, 44, skin);
    frect(x + 4, y + 2, 40, 9, RGB(200, 170, 90)); /* hair */
    int hp = P.health;
    int dead = hp <= 0;
    frect(x + 12, y + 18, 7, 4, dead ? RGB(30, 30, 30) : RGB(40, 90, 160));
    frect(x + 29, y + 18, 7, 4, dead ? RGB(30, 30, 30) : RGB(40, 90, 160));
    if (hp > 66) { frect(x + 15, y + 34, 18, 3, RGB(130, 50, 50)); frect(x + 13, y + 32, 3, 3, RGB(130, 50, 50)); frect(x + 32, y + 32, 3, 3, RGB(130, 50, 50)); }
    else if (hp > 33) frect(x + 15, y + 34, 18, 3, RGB(130, 50, 50));
    else { frect(x + 15, y + 33, 18, 5, RGB(100, 20, 20)); frect(x + 6, y + 8, 4, 22, RGB(170, 20, 20)); }
    if (hp <= 66) frect(x + 34, y + 10, 6, 10, RGB(170, 20, 20));
    if (dead) { frect(x + 12, y + 18, 24, 3, RGB(160, 20, 20)); }
    if (P.hurt > 0.2f) frect(x + 10, y + 14, 8, 2, RGB(40, 20, 20));
}

static void render_hud(void) {
    int y0 = VIEW_H;
    for (int y = 0; y < BAR_H; y++) {
        int v = 36 + y / 3;
        frect(0, y0 + y, SCR_W, 1, RGB(v - 8, v, v + 40));
    }
    frect(0, y0, SCR_W, 3, RGB(150, 160, 230));
    frect(0, y0 + 3, SCR_W, 1, RGB(10, 10, 30));
    char b[32];
    uint32_t lab = RGB(190, 200, 255), val = RGB(255, 235, 120);
    draw_text(12, y0 + 10, 2, lab, "FLOOR");
    snprintf(b, sizeof b, "%d", level);
    draw_text_sh(14, y0 + 30, 4, val, b);
    draw_text(80, y0 + 10, 2, lab, "SCORE");
    snprintf(b, sizeof b, "%d", P.score);
    draw_text_sh(80, y0 + 30, 4, val, b);
    draw_text(200, y0 + 10, 2, lab, "LIVES");
    snprintf(b, sizeof b, "%d", P.lives);
    draw_text_sh(206, y0 + 30, 4, val, b);
    draw_face(SCR_W / 2 - 24, y0 + 20);
    draw_text(382, y0 + 10, 2, lab, "HEALTH");
    snprintf(b, sizeof b, "%d%%", P.health);
    draw_text_sh(382, y0 + 30, 4, P.health > 33 ? val : RGB(255, 80, 70), b);
    draw_text(492, y0 + 10, 2, lab, "AMMO");
    snprintf(b, sizeof b, "%d", P.ammo);
    draw_text_sh(492, y0 + 30, 4, P.ammo > 0 || P.weapon == W_KNIFE ? val : RGB(255, 80, 70), b);
    /* weapon icon */
    const uint32_t *t = wpntex[P.weapon * 2];
    for (int x = 0; x < TEX; x++)
        for (int y = 0; y < TEX; y++) {
            uint32_t px = t[x * TEX + y];
            if ((px >> 24) && y0 + 8 + y < SCR_H) fb[(y0 + 8 + y) * SCR_W + 566 + x] = px;
        }
    static const char *wn[3] = { "KNIFE", "PISTOL", "GUN" };
    draw_text(566, y0 + 68 - 2, 1, lab, wn[P.weapon]);
}

static void render_overlay_fx(void) {
    if (P.hurt > 0) {
        int a = (int)(clampf(P.hurt, 0, 0.5f) * 2 * 150);
        frect_a(0, 0, SCR_W, VIEW_H, RGB(200, 0, 0), a);
    }
    if (P.flash > 0) {
        int a = (int)(clampf(P.flash, 0, 0.3f) / 0.3f * 70);
        frect_a(0, 0, SCR_W, VIEW_H, RGB(255, 235, 120), a);
    }
    if (P.wfire > 0 && P.weapon != W_KNIFE) frect_a(0, 0, SCR_W, VIEW_H, RGB(255, 220, 140), 22);
}

static void render_automap(void) {
    int sc = 5, ox = 10, oy = 10;
    frect_a(ox - 4, oy - 4, MW * sc + 8, MH * sc + 8, RGB(0, 0, 0), 150);
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            if (!seen[y][x]) continue;
            uint32_t c = 0;
            switch (map[y][x]) {
            case T_EMPTY: c = pillar[y][x] ? RGB(120, 120, 120) : RGB(55, 60, 80); break;
            case T_DOOR: c = RGB(230, 190, 40); break;
            case T_EXIT: c = RGB(40, 230, 90); break;
            default: c = RGB(150, 150, 165);
            }
            frect(ox + x * sc, oy + y * sc, sc, sc, c);
        }
    int px = ox + (int)(P.x * sc), py = oy + (int)(P.y * sc);
    frect(px - 2, py - 2, 5, 5, RGB(255, 60, 60));
    for (int i = 1; i < 8; i++) fpx(px + (int)(P.dirx * i), py + (int)(P.diry * i), RGB(255, 200, 200));
}

static void center_screen_text(const char *title, uint32_t col, const char *sub) {
    draw_text_c(SCR_W / 2, 120, 8, col, title);
    if (sub && ((int)(state_t * 2) & 1) == 0) draw_text_c(SCR_W / 2, 300, 3, RGB(240, 240, 240), sub);
}

static void render_game(void) {
    Cam c = { P.x, P.y, P.ang, P.dirx, P.diry, P.planex, P.planey };
    if (gstate == S_TITLE) {
        float a = game_time * 0.25f;
        c = (Cam){ start_x + 0.5f, start_y + 0.5f, a, cosf(a), sinf(a), -sinf(a) * 0.66f, cosf(a) * 0.66f };
        render_world(&c);
        P.hurt = P.flash = 0;
        frect_a(0, 0, SCR_W, SCR_H, RGB(0, 0, 0), 110);
        frect(0, VIEW_H, SCR_W, BAR_H, RGB(0, 0, 0));
        frect_a(0, 60, SCR_W, 130, RGB(0, 0, 0), 170);
        draw_text_c(SCR_W / 2, 74, 12, RGB(200, 20, 20), "WOLFY 3D");
        draw_text_c(SCR_W / 2, 176, 2, RGB(210, 210, 220), "A RAYCASTING SHOOTER");
        if (((int)(game_time * 2) & 1) == 0) draw_text_c(SCR_W / 2, 250, 3, RGB(255, 235, 120), "PRESS ENTER TO START");
        draw_text_c(SCR_W / 2, 300, 2, RGB(180, 190, 230), "WASD MOVE   MOUSE LOOK   CLICK OR CTRL FIRE");
        draw_text_c(SCR_W / 2, 320, 2, RGB(180, 190, 230), "SPACE OPEN DOORS   1 2 3 WEAPONS   SHIFT RUN");
        draw_text_c(SCR_W / 2, 340, 2, RGB(180, 190, 230), "TAB MAP   M MUSIC   ESC PAUSE");
        draw_text_c(SCR_W / 2, 372, 2, RGB(140, 150, 190), "FIND THE GREEN EXIT SIGN ON EACH FLOOR");
        return;
    }
    if (gstate == S_DYING) {
        /* camera sinks as the player falls */
        float k = clampf(state_t / 1.2f, 0, 1);
        c.y += 0; (void)k;
    }
    render_world(&c);
    if (gstate == S_PLAY || gstate == S_PAUSE) render_weapon();
    render_overlay_fx();
    if (gstate == S_DYING) {
        int a = (int)(clampf(state_t / 1.5f, 0, 1) * 200);
        frect_a(0, 0, SCR_W, VIEW_H, RGB(150, 0, 0), a);
    }
    render_hud();
    if (automap && (gstate == S_PLAY || gstate == S_PAUSE)) render_automap();
    if (gstate == S_LEVELDONE) {
        frect_a(0, 0, SCR_W, SCR_H, RGB(0, 0, 0), 200);
        char b[64];
        snprintf(b, sizeof b, "FLOOR %d COMPLETE", level);
        draw_text_c(SCR_W / 2, 50, 6, RGB(255, 235, 120), b);
        snprintf(b, sizeof b, "KILLS     %d/%d", got_kills, total_enemies);
        draw_text_c(SCR_W / 2, 140, 3, RGB(230, 230, 240), b);
        snprintf(b, sizeof b, "TREASURE  %d/%d", got_treasure, total_treasure);
        draw_text_c(SCR_W / 2, 175, 3, RGB(230, 230, 240), b);
        int tsec = (int)level_time;
        snprintf(b, sizeof b, "TIME      %d:%02d", tsec / 60, tsec % 60);
        draw_text_c(SCR_W / 2, 210, 3, RGB(230, 230, 240), b);
        snprintf(b, sizeof b, "SCORE     %d", P.score);
        draw_text_c(SCR_W / 2, 255, 3, RGB(255, 235, 120), b);
        if (((int)(state_t * 2) & 1) == 0) draw_text_c(SCR_W / 2, 330, 3, RGB(200, 200, 255), "PRESS ENTER");
    }
    if (gstate == S_GAMEOVER) {
        frect_a(0, 0, SCR_W, SCR_H, RGB(40, 0, 0), 220);
        char b[64];
        center_screen_text("GAME OVER", RGB(220, 30, 30), "PRESS ENTER");
        snprintf(b, sizeof b, "FINAL SCORE %d", P.score);
        draw_text_c(SCR_W / 2, 230, 3, RGB(255, 235, 120), b);
    }
    if (gstate == S_WIN) {
        frect_a(0, 0, SCR_W, SCR_H, RGB(0, 0, 40), 215);
        char b[64];
        center_screen_text("VICTORY!", RGB(255, 220, 60), "PRESS ENTER");
        draw_text_c(SCR_W / 2, 205, 3, RGB(230, 230, 240), "YOU ESCAPED THE FORTRESS");
        snprintf(b, sizeof b, "FINAL SCORE %d", P.score);
        draw_text_c(SCR_W / 2, 240, 3, RGB(255, 235, 120), b);
    }
    if (gstate == S_PAUSE) {
        frect_a(0, 0, SCR_W, SCR_H, RGB(0, 0, 0), 150);
        draw_text_c(SCR_W / 2, 140, 6, RGB(255, 235, 120), "PAUSED");
        draw_text_c(SCR_W / 2, 220, 2, RGB(220, 220, 230), "ESC RESUME   Q QUIT TO TITLE");
    }
}

/* ------------------------------------------------------------------ */
/* main update                                                        */
/* ------------------------------------------------------------------ */
static void game_update(float dt, const Input *in) {
    game_time += dt;
    state_t += dt;
    switch (gstate) {
    case S_TITLE:
        if (in->enter) { play_sfx(SND_MENU, 0.8f, 0); new_game(); }
        break;
    case S_PLAY:
        level_time += dt;
        update_player(dt, in);
        update_doors(dt);
        update_enemies(dt);
        if (in->back) { gstate = S_PAUSE; }
        break;
    case S_PAUSE:
        if (in->back) gstate = S_PLAY;
        if (in->select == 9) { gstate = S_TITLE; music_play(0); start_level(1); }
        break;
    case S_DYING:
        update_doors(dt);
        P.hurt = 0.3f;
        if (state_t > 2.4f) {
            P.lives--;
            if (P.lives <= 0) {
                gstate = S_GAMEOVER;
                state_t = 0;
                music_play(0);
            } else {
                P.ammo = 8;
                P.weapon = W_PISTOL;
                start_level(level);
                gstate = S_PLAY;
                state_t = 0;
            }
        }
        break;
    case S_LEVELDONE:
        if (state_t > 0.8f && in->enter) {
            if (level >= NLEVELS) { gstate = S_WIN; state_t = 0; music_play(0); }
            else { start_level(level + 1); gstate = S_PLAY; state_t = 0; }
        }
        break;
    case S_GAMEOVER:
    case S_WIN:
        if (state_t > 0.8f && in->enter) { gstate = S_TITLE; start_level(1); }
        break;
    }
}

/* ------------------------------------------------------------------ */
/* autopilot (used by --demo and the self-test)                       */
/* ------------------------------------------------------------------ */
static float angdiff(float a, float b) {
    float d = fmodf(a - b + PI_F, TAU_F);
    if (d < 0) d += TAU_F;
    return d - PI_F;
}
static void autopilot(Input *in, float dt) {
    memset(in, 0, sizeof *in);
    /* target the nearest visible enemy */
    Enemy *tgt = NULL;
    float bd = 1e9f;
    for (int i = 0; i < nen; i++) {
        if (en[i].state == ES_DEAD) continue;
        float dx = en[i].x - P.x, dy = en[i].y - P.y, d = sqrtf(dx * dx + dy * dy);
        if (d < 12.0f && d < bd && los(P.x, P.y, en[i].x, en[i].y)) { tgt = &en[i]; bd = d; }
    }
    float want = P.ang;
    int moving = 1;
    if (tgt) {
        want = atan2f(tgt->y - P.y, tgt->x - P.x);
        float err = angdiff(want, P.ang);
        if (fabsf(err) < 0.06f) in->fire = 1;
        if (bd < 5.0f) moving = 0;
        if (P.weapon == W_PISTOL && P.has_mg) P.weapon = W_MG;
    }
    int tx = (int)P.x, ty = (int)P.y;
    if (!tgt || moving) {
        if (tx == exit_ax && ty == exit_ay) {
            if (!tgt) want = atan2f(exit_y + 0.5f - P.y, exit_x + 0.5f - P.x);
            if (fabsf(angdiff(atan2f(exit_y + 0.5f - P.y, exit_x + 0.5f - P.x), P.ang)) < 0.15f) in->use = 1;
            moving = 0;
        } else if (fieldE[ty][tx] >= 0) {
            int bx = tx, by = ty, b = fieldE[ty][tx];
            static const int dx4[4] = { 1, -1, 0, 0 }, dy4[4] = { 0, 0, 1, -1 };
            for (int k = 0; k < 4; k++) {
                int nx = tx + dx4[k], ny = ty + dy4[k];
                if (in_map(nx, ny) && fieldE[ny][nx] >= 0 && fieldE[ny][nx] < b) { b = fieldE[ny][nx]; bx = nx; by = ny; }
            }
            if (!tgt) want = atan2f(by + 0.5f - P.y, bx + 0.5f - P.x);
            if (map[by][bx] == T_DOOR && dopen[by][bx] < 0.85f) {
                in->use = 1;
                if (dopen[by][bx] < 0.3f && fabsf(P.x - (bx + 0.5f)) + fabsf(P.y - (by + 0.5f)) < 1.3f) moving = 0;
            }
        }
    }
    float err = angdiff(want, P.ang);
    float maxturn = 5.0f * dt;
    float tr = clampf(err, -maxturn, maxturn);
    in->mouse_turn = tr;
    if (moving && fabsf(err) < 0.5f) in->fwd = 1.0f;
    (void)dt;
}

/* ------------------------------------------------------------------ */
/* headless helpers: screenshot, wav export, self-test                */
/* ------------------------------------------------------------------ */
static int save_screenshot(const char *path) {
    SDL_Surface *s = SDL_CreateRGBSurfaceFrom(fb, SCR_W, SCR_H, 32, SCR_W * 4, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!s) return -1;
    int r = SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
    return r;
}

static int export_wav(const char *path, int track, float seconds) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    int frames = (int)(seconds * RATE);
    uint32_t dlen = (uint32_t)frames * 4, v32;
    uint16_t v16;
    fwrite("RIFF", 1, 4, f); v32 = 36 + dlen; fwrite(&v32, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v32 = 16; fwrite(&v32, 4, 1, f);
    v16 = 1; fwrite(&v16, 2, 1, f); v16 = 2; fwrite(&v16, 2, 1, f);
    v32 = RATE; fwrite(&v32, 4, 1, f); v32 = RATE * 4; fwrite(&v32, 4, 1, f);
    v16 = 4; fwrite(&v16, 2, 1, f); v16 = 16; fwrite(&v16, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dlen, 4, 1, f);
    music_play(track);
    static int16_t buf[2048];
    for (int done = 0; done < frames;) {
        int n = frames - done > 1024 ? 1024 : frames - done;
        mix_audio(buf, n);
        fwrite(buf, 4, (size_t)n, f);
        done += n;
    }
    fclose(f);
    return 0;
}

static int selftest(void) {
    int bad = 0;
    for (int L = 1; L <= NLEVELS; L++) {
        gen_level(L);
        short f[MAXMAP][MAXMAP];
        bfs(f, start_x, start_y);
        int unreachable = 0;
        for (int i = 0; i < nitems; i++)
            if (items[i].kind != I_PILLAR && f[(int)items[i].y][(int)items[i].x] < 0) unreachable++;
        for (int i = 0; i < nen; i++)
            if (f[(int)en[i].y][(int)en[i].x] < 0) unreachable++;
        int exit_ok = f[exit_ay][exit_ax] >= 0 && map[exit_y][exit_x] == T_EXIT;
        printf("level %d: %dx%d  enemies=%d treasure=%d items=%d exit_dist=%d unreachable=%d %s\n", L, MW, MH, total_enemies,
               total_treasure, nitems, exit_ok ? f[exit_ay][exit_ax] : -1, unreachable, (exit_ok && !unreachable) ? "OK" : "FAIL");
        if (!exit_ok || unreachable) bad++;
    }
    return bad;
}

static int run_demo(int seconds, const char *shotprefix, int shot_every, int start_lvl) {
    gstate = S_TITLE;
    P.ammo = 8;
    new_game();
    if (start_lvl > 1) start_level(start_lvl);
    godmode = 1;
    int frames = seconds * 30;
    int completed = 0;
    for (int fr = 0; fr < frames; fr++) {
        Input in;
        autopilot(&in, 1.0f / 30);
        if (gstate == S_LEVELDONE) { in = (Input){ 0 }; in.enter = 1; if (state_t < 0.05f) { completed++; printf("  floor %d done at t=%.1fs kills %d/%d treasure %d/%d score %d\n", level, game_time, got_kills, total_enemies, got_treasure, total_treasure, P.score); } }
        else if (gstate == S_WIN) { printf("  VICTORY at t=%.1fs score %d\n", game_time, P.score); return 0; }
        game_update(1.0f / 30, &in);
        if (shotprefix && shot_every && fr % shot_every == 0) {
            render_game();
            char name[256];
            snprintf(name, sizeof name, "%s%04d.bmp", shotprefix, fr / shot_every);
            save_screenshot(name);
        }
    }
    printf("  demo ended in state %d on floor %d (completed %d) hp %d ammo %d wpn %d pos %.1f,%.1f lives %d\n", gstate, level, completed, P.health, P.ammo, P.weapon, P.x, P.y, P.lives);
    return gstate == S_WIN ? 0 : 1;
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */
static void usage(void) {
    printf("WOLFY 3D\n"
           "  ./wolfy                 play\n"
           "  ./wolfy --fullscreen    start fullscreen\n"
           "  ./wolfy --god           invincible (testing)\n"
           "  ./wolfy --selftest      check generated levels are completable\n"
           "  ./wolfy --demo SECS [PREFIX [EVERY [LEVEL]]]\n"
           "                          headless autopilot run, optional BMP screenshots\n"
           "  ./wolfy --wav FILE TRACK SECS   export music track 0 (title) / 1 (game)\n");
}

int main(int argc, char **argv) {
    int fullscreen = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(); return 0; }
        if (!strcmp(argv[i], "--fullscreen")) fullscreen = 1;
        if (!strcmp(argv[i], "--god")) godmode = 1;
    }
    int headless = 0;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--selftest") || !strcmp(argv[i], "--demo") || !strcmp(argv[i], "--wav")) headless = 1;

    rng_s = (uint32_t)SDL_GetTicks() * 2654435761u + 1;
    gen_walls();
    gen_sprites();
    gen_weapons();
    gen_sounds();
    render_track(0);
    render_track(1);

    if (headless) {
        if (SDL_Init(SDL_INIT_TIMER) < 0) { /* only needed for SDL_SaveBMP/ticks */ }
        for (int i = 1; i < argc; i++) {
            if (!strcmp(argv[i], "--selftest")) return selftest() ? 1 : 0;
            if (!strcmp(argv[i], "--wav") && i + 3 < argc) {
                int r = export_wav(argv[i + 1], atoi(argv[i + 2]), (float)atof(argv[i + 3]));
                printf("wrote %s (%d)\n", argv[i + 1], r);
                return r ? 1 : 0;
            }
            if (!strcmp(argv[i], "--demo") && i + 1 < argc) {
                int secs = atoi(argv[i + 1]);
                const char *pre = i + 2 < argc ? argv[i + 2] : NULL;
                int every = i + 3 < argc ? atoi(argv[i + 3]) : 0;
                int lvl = i + 4 < argc ? atoi(argv[i + 4]) : 1;
                return run_demo(secs, pre, every, lvl);
            }
        }
        usage();
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *win = SDL_CreateWindow("WOLFY 3D", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCR_W * 2, SCR_H * 2,
                                       SDL_WINDOW_RESIZABLE | (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
    if (!win) { fprintf(stderr, "window: %s\n", SDL_GetError()); return 1; }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { fprintf(stderr, "renderer: %s\n", SDL_GetError()); return 1; }
    SDL_RenderSetLogicalSize(ren, SCR_W, SCR_H);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCR_W, SCR_H);

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = audio_cb;
    adev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!adev) fprintf(stderr, "audio unavailable: %s (continuing silently)\n", SDL_GetError());
    else SDL_PauseAudioDevice(adev, 0);

    start_level(1);
    P.lives = 3;
    gstate = S_TITLE;
    music_play(0);
    SDL_SetRelativeMouseMode(SDL_FALSE);

    int running = 1;
    uint32_t last = SDL_GetTicks();
    while (running) {
        Input in;
        memset(&in, 0, sizeof in);
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT: running = 0; break;
            case SDL_MOUSEMOTION:
                if (gstate == S_PLAY) in.mouse_turn += ev.motion.xrel * 0.0028f;
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (ev.button.button == SDL_BUTTON_LEFT && gstate == S_PLAY) {
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                }
                break;
            case SDL_KEYDOWN:
                if (ev.key.repeat) break;
                switch (ev.key.keysym.sym) {
                case SDLK_RETURN:
                    if (ev.key.keysym.mod & KMOD_ALT) {
                        Uint32 fl = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                        SDL_SetWindowFullscreen(win, fl ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                    } else in.enter = 1;
                    break;
                case SDLK_KP_ENTER: in.enter = 1; break;
                case SDLK_F11: {
                    Uint32 fl = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                    SDL_SetWindowFullscreen(win, fl ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                    break;
                }
                case SDLK_ESCAPE:
                    if (gstate == S_TITLE) running = 0;
                    else in.back = 1;
                    break;
                case SDLK_q:
                    if (gstate == S_PAUSE) in.select = 9;
                    break;
                case SDLK_SPACE:
                case SDLK_e: in.use = 1; break;
                case SDLK_1: in.select = 1; break;
                case SDLK_2: in.select = 2; break;
                case SDLK_3: in.select = 3; break;
                case SDLK_TAB: automap = !automap; break;
                case SDLK_m:
                    music_on = !music_on;
                    break;
                }
                break;
            case SDL_WINDOWEVENT:
                if (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST && gstate == S_PLAY) {
                    in.back = 1;
                }
                break;
            }
        }
        const Uint8 *ks = SDL_GetKeyboardState(NULL);
        if (ks[SDL_SCANCODE_W] || ks[SDL_SCANCODE_UP]) in.fwd += 1;
        if (ks[SDL_SCANCODE_S] || ks[SDL_SCANCODE_DOWN]) in.fwd -= 1;
        if (ks[SDL_SCANCODE_D]) in.strafe += 1;
        if (ks[SDL_SCANCODE_A]) in.strafe -= 1;
        if (ks[SDL_SCANCODE_RIGHT]) in.turn += 1;
        if (ks[SDL_SCANCODE_LEFT]) in.turn -= 1;
        if (ks[SDL_SCANCODE_LSHIFT] || ks[SDL_SCANCODE_RSHIFT]) in.run = 1;
        if (ks[SDL_SCANCODE_LCTRL] || ks[SDL_SCANCODE_RCTRL]) in.fire = 1;
        if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT) && gstate == S_PLAY) in.fire = 1;
        if (ks[SDL_SCANCODE_SPACE] == 0 && ks[SDL_SCANCODE_E] == 0) { /* use is edge-triggered above */ }

        uint32_t now = SDL_GetTicks();
        float dt = (now - last) / 1000.0f;
        last = now;
        if (dt > 0.05f) dt = 0.05f;
        int prev_state = gstate;
        game_update(dt, &in);
        if (gstate == S_PAUSE) SDL_SetRelativeMouseMode(SDL_FALSE);
        if (prev_state != S_PLAY && gstate == S_PLAY) SDL_SetRelativeMouseMode(SDL_TRUE);
        if (gstate != S_PLAY && gstate != S_DYING) { if (SDL_GetRelativeMouseMode()) SDL_SetRelativeMouseMode(SDL_FALSE); }

        render_game();
        SDL_UpdateTexture(tex, NULL, fb, SCR_W * 4);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }
    if (adev) SDL_CloseAudioDevice(adev);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
