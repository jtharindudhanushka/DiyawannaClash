#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <string>

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

// --- Structs and Enums ---
typedef enum GameScreen { MENU, GAMEPLAY, GAME_OVER } GameScreen;

struct Player {
    Rectangle rec;
    int lives;
    float recoilTimer;
};

struct Enemy {
    Rectangle rec;
    Vector2 velocity;
    int health;
    int type; // 0, 1, or 2
    bool active;
};

struct Projectile {
    Rectangle rec;
    Vector2 velocity;
    bool active;
};

struct Explosion {
    Rectangle rec; // Use a rectangle for scaling
    float frameTimer;
    int currentFrame;
    bool active;
};

struct ScrollingLayer {
    Texture2D texture;
    float baseSpeed; // The initial speed
    float x;
};

// --- Global Variables ---
const int screenWidth = 1280;
const int screenHeight = 720;
// With the above lines we can easily change the screen resolutions

// --- Scaling Constants ---
// We faced a problem of scales of assets we used. So we used a global variable to control the scales of each asset more easily
const float playerScale = 0.2f;
const float enemyScale = 0.18f;
const float lifeIconScale = 0.2f;
const float baseSpeeds[6] = { 10.0f, 15.0f, 20.0f, 25.0f, 80.0f, 120.0f };

GameScreen currentScreen = MENU;
int score = 0;
float enemySpawnTimer = 0.0f;
float shootCooldown = 0.0f;

// --- Difficulty Scaling Variables ---
// When the time passes this will gradually increase the games difiiculty withina threshhold
float gameTime = 0.0f;
float difficultyTimer = 0.0f;
float minSpawnTime = 1.5f;
float maxSpawnTime = 3.5f;

Player player;
std::vector<Enemy> enemies;
std::vector<Projectile> projectiles;
std::vector<Explosion> explosions;
ScrollingLayer backgrounds[6];

Texture2D menuScreenTexture, lifeTexture, gameOverTexture, playerTexture;
Texture2D enemyTextures[3];
Texture2D explosionFrames[11];
Font gameFont;
Music menuMusic, gameMusic, gameOverMusic;
Sound shootSound, explosionSound, playerDieSound;

// --- Function Declarations ---
void UpdateDrawFrame(void);
void ResetGame(void);
void DrawTextStroked(const char *text, int posX, int posY, int fontSize, Color color, Color strokeColor);

// --- Main Entry Point ---
int main(void) {
    InitWindow(screenWidth, screenHeight, "Diyawanna Clash");
    InitAudioDevice();
    SetTargetFPS(60);
    // The game is intended to run at 60fps to feel more smooth

    // --- Asset Loading ---
    // To make it easy for our group to load assets we loaded them at once
    menuScreenTexture = LoadTexture("assets/menuscreen.png");
    lifeTexture = LoadTexture("assets/life.png");
    gameOverTexture = LoadTexture("assets/gameover.png");
    playerTexture = LoadTexture("assets/player.png");
    enemyTextures[0] = LoadTexture("assets/enemy (1).png");
    enemyTextures[1] = LoadTexture("assets/enemy (2).png");
    enemyTextures[2] = LoadTexture("assets/enemy (3).png");
    gameFont = LoadFont("assets/arial.ttf");
    menuMusic = LoadMusicStream("assets/Audio/menu_music.ogg");
    gameMusic = LoadMusicStream("assets/Audio/game_music.ogg");
    gameOverMusic = LoadMusicStream("assets/Audio/game_over.ogg");
    shootSound = LoadSound("assets/Audio/player_shoot.ogg");
    explosionSound = LoadSound("assets/Audio/enemy_explosion.ogg");
    playerDieSound = LoadSound("assets/Audio/player_die.ogg");

    //The explosion after each enemy dies and parallax background is handled by using seperate png layers.
    for (int i = 0; i < 11; i++) {
        explosionFrames[i] = LoadTexture(TextFormat("assets/Explosion/frame%04i.png", i));
    }
    for (int i = 0; i < 6; i++) {
        backgrounds[i] = { LoadTexture(TextFormat("assets/Background/%d.png", i + 1)), baseSpeeds[i], 0.0f };
    }
    
    ResetGame();
    
#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    while (!WindowShouldClose()) {
        UpdateDrawFrame();
    }
#endif

    // --- De-Initialization ---
    UnloadTexture(menuScreenTexture);
    UnloadTexture(lifeTexture);
    UnloadTexture(gameOverTexture);
    UnloadTexture(playerTexture);
    for(int i=0; i<3; i++) UnloadTexture(enemyTextures[i]);
    for(int i=0; i<11; i++) UnloadTexture(explosionFrames[i]);
    for(int i=0; i<6; i++) UnloadTexture(backgrounds[i].texture);
    UnloadFont(gameFont);
    UnloadMusicStream(menuMusic);
    UnloadMusicStream(gameMusic);
    UnloadMusicStream(gameOverMusic);
    UnloadSound(shootSound);
    UnloadSound(explosionSound);
    UnloadSound(playerDieSound);

    CloseAudioDevice();
    CloseWindow();

    return 0;
}

void ResetGame(void) {
    player.rec.width = playerTexture.width * playerScale;
    player.rec.height = playerTexture.height * playerScale;
    player.rec.x = 100;
    player.rec.y = screenHeight / 2.0f - player.rec.height / 2;
    player.lives = 3;
    player.recoilTimer = 0.0f;
    
    score = 0;
    enemies.clear();
    projectiles.clear();
    explosions.clear();
    
    gameTime = 0.0f;
    difficultyTimer = 0.0f;
    minSpawnTime = 1.5f;
    maxSpawnTime = 3.5f;
    enemySpawnTimer = 2.0f;
    
    StopMusicStream(gameOverMusic);
    StopMusicStream(gameMusic);
    PlayMusicStream(menuMusic);
}

void UpdateDrawFrame(void) {
    float dt = GetFrameTime();

    switch(currentScreen) {
        case MENU: {
            UpdateMusicStream(menuMusic);
            Rectangle button = { screenWidth/2.0f - 150, 500, 300, 60 };
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), button)) {
                ResetGame();
                currentScreen = GAMEPLAY;
                StopMusicStream(menuMusic);
                PlayMusicStream(gameMusic);
            }
        } break;
        case GAMEPLAY: {
            UpdateMusicStream(gameMusic);
            
            gameTime += dt;
            difficultyTimer += dt;
            if (difficultyTimer > 8.0f) {
                if (minSpawnTime > 0.4f) minSpawnTime -= 0.1f;
                if (maxSpawnTime > 1.0f) maxSpawnTime -= 0.15f;
                difficultyTimer = 0.0f;
            }
            float speedMultiplier = 1.0f + (gameTime / 45.0f);

            for (int i = 0; i < 6; i++) {
                backgrounds[i].x -= (backgrounds[i].baseSpeed * speedMultiplier) * dt;
                if (backgrounds[i].x <= -backgrounds[i].texture.width) backgrounds[i].x = 0;
            }

            player.rec.y = GetMousePosition().y - player.rec.height / 2;
            if (player.rec.y < 0) player.rec.y = 0;
            if (player.rec.y > screenHeight - player.rec.height) player.rec.y = screenHeight - player.rec.height;
            
            if (player.recoilTimer > 0) player.recoilTimer -= dt;

            if (shootCooldown > 0) shootCooldown -= dt;
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && shootCooldown <= 0) {
                projectiles.push_back({
                    { player.rec.x + player.rec.width, player.rec.y + player.rec.height / 2, 15, 5 },
                    { 900.0f, 0.0f }, true
                });
                shootCooldown = 0.15f;
                player.recoilTimer = 0.1f;
                PlaySound(shootSound);
            }

            enemySpawnTimer -= dt;
            if (enemySpawnTimer <= 0) {
                int type = GetRandomValue(0, 2);
                float width = enemyTextures[type].width * enemyScale;
                float height = enemyTextures[type].height * enemyScale;
                enemies.push_back({
                    { (float)screenWidth, (float)GetRandomValue(50, screenHeight - 50 - height), width, height },
                    { (float)GetRandomValue(-300, -150) * speedMultiplier, 0.0f }, 2, type, true
                });
                enemySpawnTimer = GetRandomValue((int)(minSpawnTime*100), (int)(maxSpawnTime*100)) / 100.0f;
            }

            for (auto& p : projectiles) if (p.active) {
                p.rec.x += p.velocity.x * dt;
                if (p.rec.x > screenWidth) p.active = false;
            }
            for (auto& e : enemies) if (e.active) {
                e.rec.x += e.velocity.x * dt;
                if (e.rec.x < -e.rec.width) e.active = false;
            }
            for (auto& p : projectiles) if (p.active) {
                for (auto& e : enemies) if (e.active && CheckCollisionRecs(p.rec, e.rec)) {
                    p.active = false;
                    e.health--;
                    if (e.health <= 0) {
                        e.active = false;
                        score += 100;
                        explosions.push_back({ {e.rec.x, e.rec.y, e.rec.width, e.rec.height}, 0.0f, 0, true });
                        PlaySound(explosionSound);
                    }
                }
            }
            for (auto& e : enemies) if (e.active && CheckCollisionRecs(player.rec, e.rec)) {
                e.active = false;
                player.lives--;
                explosions.push_back({ {e.rec.x, e.rec.y, e.rec.width, e.rec.height}, 0.0f, 0, true });
                PlaySound(playerDieSound);
            }
            for (auto& ex : explosions) if (ex.active) {
                ex.frameTimer += dt;
                if (ex.frameTimer >= 0.5f / 11.0f) {
                    ex.frameTimer = 0;
                    ex.currentFrame++;
                    if (ex.currentFrame > 10) ex.active = false;
                }
            }
            
            if (player.lives <= 0) {
                currentScreen = GAME_OVER;
                StopMusicStream(gameMusic);
                PlayMusicStream(gameOverMusic);
            }

        } break;
        case GAME_OVER: {
            UpdateMusicStream(gameOverMusic);
            if (IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                currentScreen = MENU;
                StopMusicStream(gameOverMusic);
            }
        } break;
    }

    // --- Drawing ---
    BeginDrawing();
    ClearBackground(BLACK);
    
    if (currentScreen == MENU) {
        DrawTexturePro(menuScreenTexture, {0,0,(float)menuScreenTexture.width, (float)menuScreenTexture.height}, {0,0, (float)screenWidth, (float)screenHeight}, {0,0}, 0, WHITE);
        Rectangle button = { screenWidth/2.0f - 150, 500, 300, 60 };
        DrawRectangleRec(button, CheckCollisionPointRec(GetMousePosition(), button) ? SKYBLUE : DARKBLUE);
        DrawText("START GAME", button.x + 45, button.y + 15, 30, WHITE);
    } else if (currentScreen == GAMEPLAY) {
        for (int i = 0; i < 6; i++) {
            DrawTexture(backgrounds[i].texture, backgrounds[i].x, 0, WHITE);
            DrawTexture(backgrounds[i].texture, backgrounds[i].x + backgrounds[i].texture.width, 0, WHITE);
        }

        Vector2 playerDrawPos = { player.rec.x, player.rec.y };
        if (player.recoilTimer > 0) {
            playerDrawPos.x -= GetRandomValue(1, 4);
            playerDrawPos.y += GetRandomValue(-2, 2);
        }
        DrawTexturePro(playerTexture, {0,0,(float)playerTexture.width, (float)playerTexture.height}, player.rec, {0,0}, 0, WHITE);

        for (auto& e : enemies) if (e.active) DrawTexturePro(enemyTextures[e.type], {0,0,(float)enemyTextures[e.type].width, (float)enemyTextures[e.type].height}, e.rec, {0,0}, 0, WHITE);
        for (auto& p : projectiles) if (p.active) DrawRectangleRec(p.rec, ORANGE);
        for (auto& ex : explosions) if (ex.active) {
            DrawTexturePro(explosionFrames[ex.currentFrame], {0,0,(float)explosionFrames[ex.currentFrame].width, (float)explosionFrames[ex.currentFrame].height}, ex.rec, {0,0}, 0, WHITE);
        }

        for (int i = 0; i < player.lives; i++) {
             DrawTextureEx(lifeTexture, { 20 + (float)(i * lifeTexture.width * lifeIconScale), 20 }, 0, lifeIconScale, WHITE);
        }
        DrawTextStroked(TextFormat("Score: %04i", score), screenWidth - 300, 20, 40, GOLD, BLACK);

    } else if (currentScreen == GAME_OVER) {
        DrawTexturePro(gameOverTexture, {0,0,(float)gameOverTexture.width, (float)gameOverTexture.height}, {0,0, (float)screenWidth, (float)screenHeight}, {0,0}, 0, WHITE);
        
        const char* scoreText = TextFormat("Your Score : %04i", score);
        Vector2 textSize = MeasureTextEx(gameFont, scoreText, 50, 5);
        DrawTextStroked(scoreText, screenWidth - textSize.x - 40, 40, 50, GOLD, DARKBLUE);
    }
    
    EndDrawing();
}

void DrawTextStroked(const char *text, int posX, int posY, int fontSize, Color color, Color strokeColor) {
    DrawTextEx(gameFont, text, {(float)posX - 2, (float)posY - 2}, fontSize, 5, strokeColor);
    DrawTextEx(gameFont, text, {(float)posX + 2, (float)posY - 2}, fontSize, 5, strokeColor);
    DrawTextEx(gameFont, text, {(float)posX - 2, (float)posY + 2}, fontSize, 5, strokeColor);
    DrawTextEx(gameFont, text, {(float)posX + 2, (float)posY + 2}, fontSize, 5, strokeColor);
    DrawTextEx(gameFont, text, {(float)posX, (float)posY}, fontSize, 5, color);
}
