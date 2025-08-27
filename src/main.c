#include <SDL.h>
#include <SDL_ttf.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#define COLUMNS 10
#define ROWS 25
#define BLOCK_SIZE 19
#define TICK_INTERVAL 300
#define NUM_SHAPES 12

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RGB;

typedef struct
{
    bool *shape;
    int x, y, w, h;
    RGB color;
} Shape;

typedef struct
{
    const bool *pattern;
    int size;
    int w, h;
} ShapeDesc;

const bool PATTERN_1[] = {0, 1, 0,
                          1, 1, 1};

const bool PATTERN_2[] = {1, 1,
                          0, 1,
                          0, 1,
                          0, 1};

const bool PATTERN_3[] = {1, 1, 1, 1,
                          0, 1, 0, 1};

const bool PATTERN_4[] = {1, 0, 0,
                          1, 1, 0,
                          0, 1, 1};

const bool PATTERN_5[] = {0, 1,
                          1, 1};

const bool PATTERN_6[] = {1, 1,
                          1, 1};

const bool PATTERN_7[] = {1,
                          1,
                          1,
                          1};

const bool PATTERN_8[] = {1, 0, 0,
                          1, 1, 1,
                          0, 0, 1};

const bool PATTERN_9[] = {1, 1, 1, 1,
                          0, 0, 1, 0};

const bool PATTERN_10[] = {0, 1, 0,
                           0, 1, 1,
                           0, 1, 0,
                           1, 1, 0};

const bool PATTERN_11[] = {1, 0, 0, 1,
                           1, 1, 1, 1};

const bool PATTERN_12[] = {0, 1, 0, 0,
                           1, 1, 1, 1,
                           0, 0, 1, 0};

const ShapeDesc shape_defs[NUM_SHAPES] = {
    {PATTERN_1, 6, 3, 2},
    {PATTERN_2, 8, 2, 4},
    {PATTERN_3, 8, 4, 2},
    {PATTERN_4, 9, 3, 3},
    {PATTERN_5, 4, 2, 2},
    {PATTERN_6, 4, 2, 2},
    {PATTERN_7, 4, 1, 4},
    {PATTERN_8, 9, 3, 3},
    {PATTERN_9, 8, 4, 2},
    {PATTERN_10, 12, 3, 4},
    {PATTERN_11, 8, 4, 2},
    {PATTERN_12, 12, 4, 3}};

Shape *shapes[NUM_SHAPES];
void initShapes(void)
{
    static int initialized = 0;
    if (initialized)
        return;
    initialized = 1;
    for (int i = 0; i < NUM_SHAPES; i++)
    {
        Shape *shape = malloc(sizeof(Shape));
        shape->x = 0;
        shape->y = 0;
        shape->w = shape_defs[i].w;
        shape->h = shape_defs[i].h;

        bool *pattern = malloc(shape->w * shape->h * sizeof(bool));
        SDL_memcpy(pattern, shape_defs[i].pattern, shape->w * shape->h * sizeof(bool));
        shape->shape = pattern;

        shape->color.r = 80 + (i * 17) % 200;
        shape->color.g = 80 + (i * 37) % 200;
        shape->color.b = 80 + (i * 57) % 200;

        shapes[i] = shape;
    }
}

Shape *getRandomShape()
{
    Shape *base = shapes[rand() % NUM_SHAPES];
    Shape *shape = malloc(sizeof(Shape));
    shape->x = COLUMNS / 2 - base->w / 2;
    shape->y = 0;
    shape->w = base->w;
    shape->h = base->h;
    shape->color = base->color;
    shape->shape = malloc(shape->w * shape->h * sizeof(bool));
    SDL_memcpy(shape->shape, base->shape, shape->w * shape->h * sizeof(bool));
    return shape;
}

SDL_Renderer *renderer = NULL;
Shape *activeShape = NULL;
Shape *reservedShape = NULL;
RGB **mat = NULL;
Uint64 last_tick;
TTF_Font *font = NULL;
TTF_Font *header_font = NULL;
int running = 1;
bool lost_flag = 0;

void clearScreen()
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
}

void renderLostMenu()
{
    if (!renderer)
        return;

    SDL_Color color = {255, 0, 0, 255};
    SDL_Surface *text_surface = TTF_RenderText_Solid(header_font, "You Lost", color);
    SDL_Texture *texture_surface = SDL_CreateTextureFromSurface(renderer, text_surface);
    SDL_Rect text_rect = {320, 240, text_surface->w, text_surface->h};

    SDL_RenderCopy(renderer, texture_surface, NULL, &text_rect);
    SDL_FreeSurface(text_surface);
}

void renderBoard()
{
    if (!mat)
        return;

    for (int i = 0; i < ROWS * COLUMNS; ++i)
    {
        int y = i / COLUMNS;
        int x = i % COLUMNS;
        if (mat[i])
            SDL_SetRenderDrawColor(renderer, mat[i]->r, mat[i]->g, mat[i]->b, 255);
        else
            SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);

        SDL_Rect cell = {x * BLOCK_SIZE, y * BLOCK_SIZE, BLOCK_SIZE - 2, BLOCK_SIZE - 2};
        SDL_RenderFillRect(renderer, &cell);
    }

    if (activeShape)
    {
        SDL_SetRenderDrawColor(renderer, activeShape->color.r, activeShape->color.g, activeShape->color.b, 255);
        for (int dy = 0; dy < activeShape->h; ++dy)
        {
            for (int dx = 0; dx < activeShape->w; ++dx)
            {
                if (activeShape->shape[dy * activeShape->w + dx])
                {
                    int y = activeShape->y + dy;
                    int x = activeShape->x + dx;
                    if (y >= 0 && y < ROWS && x >= 0 && x < COLUMNS)
                    {
                        SDL_Rect cell = {x * BLOCK_SIZE, y * BLOCK_SIZE, BLOCK_SIZE - 2, BLOCK_SIZE - 2};
                        SDL_RenderFillRect(renderer, &cell);
                    }
                }
            }
        }
    }
}

void renderHud()
{
    int startX = COLUMNS * BLOCK_SIZE + 50;
    int startY = 50;

    if (reservedShape && font)
    {
        SDL_Color color = {255, 255, 255, 255};
        SDL_Surface *text_surface = TTF_RenderText_Solid(font, "Next Shape:", color);
        SDL_Texture *texture_surface = SDL_CreateTextureFromSurface(renderer, text_surface);
        SDL_Rect text_rect = {startX, startY, text_surface->w, text_surface->h};

        startY += text_surface->h + 5;
        startX += text_surface->w / 2;

        SDL_RenderCopy(renderer, texture_surface, NULL, &text_rect);
        SDL_FreeSurface(text_surface);
        // SDL_DestroyTexture(texture_surface); // Destroy whole renderer

        SDL_SetRenderDrawColor(renderer, reservedShape->color.r, reservedShape->color.g, reservedShape->color.b, 255);
        for (int i = 0; i < reservedShape->h; ++i)
        {
            int y = startY + (i * BLOCK_SIZE);
            for (int j = 0; j < reservedShape->w; ++j)
            {
                int x = startX + (j * BLOCK_SIZE);
                if (reservedShape->shape[i * reservedShape->w + j])
                {
                    SDL_Rect cell = {x, y, BLOCK_SIZE - 2, BLOCK_SIZE - 2};
                    SDL_RenderFillRect(renderer, &cell);
                }
            }
        }
    }
}

void saveShape()
{
    if (!mat || !activeShape)
        return;

    for (int dy = 0; dy < activeShape->h; ++dy)
    {
        for (int dx = 0; dx < activeShape->w; ++dx)
        {
            if (activeShape->shape[dy * activeShape->w + dx])
            {
                int y = activeShape->y + dy;
                int x = activeShape->x + dx;
                if (y >= 0 && y < ROWS && x >= 0 && x < COLUMNS)
                {
                    if (!mat[y * COLUMNS + x])
                        mat[y * COLUMNS + x] = malloc(sizeof(RGB));
                    *mat[y * COLUMNS + x] = activeShape->color;
                }
            }
        }
    }
}

bool isCollided()
{
    if (!mat || !activeShape)
        return false;

    if (activeShape->x < 0 || activeShape->x + activeShape->w > COLUMNS)
        return true;
    if (activeShape->y < 0 || activeShape->y + activeShape->h > ROWS)
        return true;

    for (int dy = 0; dy < activeShape->h; ++dy)
    {
        int y = activeShape->y + dy;
        for (int dx = 0; dx < activeShape->w; ++dx)
        {
            int x = activeShape->x + dx;
            if (y >= 0 && y < ROWS && x >= 0 && x < COLUMNS)
            {
                if (activeShape->shape[dy * activeShape->w + dx])
                {
                    if (mat[y * COLUMNS + x])
                        return true;
                }
            }
        }
    }

    return false;
}

bool willCollide()
{
    if (!activeShape || !activeShape)
        return false;

    activeShape->y++;
    bool result = isCollided();
    activeShape->y--;
    return result;
}

void rotateShape()
{
    if (!activeShape || !activeShape)
        return;

    int old_x = activeShape->x, old_y = activeShape->y, old_w = activeShape->w, old_h = activeShape->h;
    bool *old_pattern = activeShape->shape;

    bool *new_pattern = (bool *)malloc(old_h * old_w * sizeof(bool));
    for (int j = 0; j < old_w; ++j)
        for (int i = 0; i < old_h; ++i)
            new_pattern[j * old_h + i] = old_pattern[(old_h - i - 1) * old_w + j];

    activeShape->w = old_h;
    activeShape->h = old_w;
    activeShape->shape = new_pattern;

    if (!isCollided())
    {
        free(old_pattern);
        return;
    }

    int kicks[3][2] = {{-1, 0}, {1, 0}, {0, -1}};
    for (int k = 0; k < 3; ++k)
    {
        activeShape->x = old_x + kicks[k][0];
        activeShape->y = old_y + kicks[k][1];
        if (!isCollided())
        {
            free(old_pattern);
            return;
        }
    }

    // rotate failed revert
    free(new_pattern);
    activeShape->shape = old_pattern;
    activeShape->x = old_x;
    activeShape->y = old_y;
    activeShape->w = old_w;
    activeShape->h = old_h;
}

void moveHandle(const SDL_Event *event)
{
    int cacheX = activeShape->x;
    int cacheY = activeShape->y;

    if (event->key.keysym.scancode == SDL_SCANCODE_A)
        activeShape->x--;
    else if (event->key.keysym.scancode == SDL_SCANCODE_D)
        activeShape->x++;
    else if (event->key.keysym.scancode == SDL_SCANCODE_S)
        activeShape->y++;
    else if (event->key.keysym.scancode == SDL_SCANCODE_R)
        rotateShape();

    if (isCollided())
    {
        activeShape->x = cacheX;
        activeShape->y = cacheY;
    }
}

void floodDestroy()
{
    bool visited[ROWS * COLUMNS] = {0};
    int queue[ROWS * COLUMNS];
    int front = 0, back = 0;
    int region_size = 0;
    RGB color = activeShape->color;

    for (int dy = 0; dy < activeShape->h; ++dy)
    {
        int y = activeShape->y + dy;
        for (int dx = 0; dx < activeShape->w; ++dx)
        {
            if (activeShape->shape[dy * activeShape->w + dx])
            {
                int x = activeShape->x + dx;
                if (y >= 0 && y < ROWS && x >= 0 && x < COLUMNS)
                {
                    int idx = y * COLUMNS + x;
                    if (mat[idx] && mat[idx]->r == color.r && mat[idx]->g == color.g && mat[idx]->b == color.b && !visited[idx])
                    {
                        visited[idx] = true;
                        queue[back++] = idx;
                        region_size++;
                    }
                }
            }
        }
    }

    while (front < back)
    {
        int idx = queue[front++];
        int y = idx / COLUMNS;
        int x = idx % COLUMNS;
        int dirs[4][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
        for (int d = 0; d < 4; ++d)
        {
            int ny = y + dirs[d][0];
            int nx = x + dirs[d][1];
            if (ny >= 0 && ny < ROWS && nx >= 0 && nx < COLUMNS)
            {
                int nidx = ny * COLUMNS + nx;
                if (!visited[nidx] && mat[nidx] && mat[nidx]->r == color.r && mat[nidx]->g == color.g && mat[nidx]->b == color.b)
                {
                    visited[nidx] = true;
                    queue[back++] = nidx;
                    region_size++;
                }
            }
        }
    }

    int shape_blocks = 0;
    for (int dy = 0; dy < activeShape->h; ++dy)
        for (int dx = 0; dx < activeShape->w; ++dx)
            if (activeShape->shape[dy * activeShape->w + dx])
                shape_blocks++;

    // check if blocks belongs to more than current shape
    if (region_size > shape_blocks)
    {
        for (int i = 0; i < ROWS * COLUMNS; ++i)
        {
            if (visited[i] && mat[i])
            {
                free(mat[i]);
                mat[i] = NULL;
            }
        }
    }
}

void game_loop()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
            running = 0;

        if (event.type == SDL_KEYDOWN)
            if (activeShape)
                moveHandle(&event);
    }

    if (lost_flag)
        renderLostMenu();
    else
    {
        Uint64 now = SDL_GetTicks();
        if (now - last_tick >= TICK_INTERVAL)
        {
            last_tick = now;

            if (activeShape)
            {
                if (!willCollide())
                    activeShape->y++;
                else
                {
                    if (activeShape->y == 0)
                        lost_flag = 1;
                    saveShape();
                    floodDestroy();
                    free(activeShape->shape);
                    free(activeShape);
                    activeShape = NULL;
                }
            }
            else
            {
                if (reservedShape)
                    activeShape = reservedShape;
                else
                    activeShape = getRandomShape();

                reservedShape = getRandomShape();
            }
        }

        clearScreen();
        renderBoard();
        renderHud();
    }
    SDL_RenderPresent(renderer);

#ifndef __EMSCRIPTEN__
    SDL_Delay(16);
#endif
}

int main(int argc, char *argv[])
{
    srand(time(NULL));

    if (TTF_Init() == -1)
    {
        SDL_Log("Couldn't initialize SDL_ttf: %s", TTF_GetError());
        return 1;
    }

    SDL_Window *window = NULL;

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return 1;
    }

    if (SDL_CreateWindowAndRenderer(640, 480, 0, &window, &renderer) < 0)
    {
        SDL_Log("Couldn't create window: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowTitle(window, "Tetris");

    mat = (RGB **)malloc(COLUMNS * ROWS * sizeof(RGB *));
    for (int i = 0; i < COLUMNS * ROWS; ++i)
    {
        mat[i] = NULL;
    }
    last_tick = SDL_GetTicks();
    font = TTF_OpenFont("./fonts/FIRACODE-VF.TTF", 18);
    header_font = TTF_OpenFont("./fonts/FIRACODE-VF.TTF", 36);

    initShapes();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(game_loop, 0, 1);
#else
    while (running)
    {
        game_loop();
    }

    for (int i = 0; i < NUM_SHAPES; ++i)
    {
        free(shapes[i]->shape);
        free(shapes[i]);
    }

    for (int i = 0; i < COLUMNS * ROWS; ++i)
    {
        if (mat[i])
            free(mat[i]);
    }
    free(mat);

    if (activeShape)
    {
        free(activeShape->shape);
        free(activeShape);
    }
    if (reservedShape)
    {
        free(reservedShape->shape);
        free(reservedShape);
    }

    TTF_CloseFont(font);
    TTF_CloseFont(header_font);
    TTF_Quit();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
#endif

    return 0;
}
