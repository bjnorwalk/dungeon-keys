#include "raylib.h"
#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>

//
// Dungeon Keys – Rogue Edition
//
// This expanded version builds on the polished typing roguelike by adding
// several classic roguelike elements: items and power‑ups, a simple shop
// between floors, boss encounters every few waves, randomly placed traps
// that must be disarmed, and persistent upgrades.  The core typing
// mechanics remain the same—you move with WASD or arrow keys and type
// the words above enemies to defeat them—but now you can also collect
// health potions, bombs and freeze scrolls, purchase permanent upgrades
// with gold earned from defeating enemies, and face off against bosses
// that require multiple long words to defeat.  High scores and upgrade
// progress are saved to disk so you can continue improving your hero over
// multiple sessions.

// Tile and map configuration
#define TILE_SIZE    32
#define MAP_WIDTH    24
#define MAP_HEIGHT   16

// The status panel at the bottom adds extra space for HUD and shop text
#define STATUS_PANEL_HEIGHT 180
#define SCREEN_WIDTH  (MAP_WIDTH * TILE_SIZE)
#define SCREEN_HEIGHT (MAP_HEIGHT * TILE_SIZE + STATUS_PANEL_HEIGHT)

// Maximum counts for various entities
#define MAX_ENEMIES       16
#define MAX_ITEMS         4
#define MAX_TRAPS         4
#define MAX_PARTICLES     150
#define MAX_FLOATING_TEXTS 50

// Game progression constants
#define WAVES_PER_FLOOR   5        // waves before a boss appears
#define BOSS_WORDS        3        // words required to defeat the boss
#define ITEM_SPAWN_INTERVAL 15.0f  // seconds between potential item spawns

// Base spawn rate and delays; modified by upgrades and difficulty
#define BASE_SPAWN_RATE   2.2f
#define BASE_ENEMY_DELAY_GOBLIN   0.75f
#define BASE_ENEMY_DELAY_SKELETON 0.60f
#define BASE_ENEMY_DELAY_WRAITH   0.45f

// Upgrade costs
#define COST_UPGRADE_HEALTH  200
#define COST_UPGRADE_MOVES   200
#define COST_UPGRADE_SLOW    200
#define COST_UPGRADE_COMBO   150

// Enumeration of game states
typedef enum {
    STATE_MENU,
    STATE_PLAY,
    STATE_SHOP,
    STATE_GAME_OVER
} GameState;

// Enemy types for colour coding and base stats
typedef enum {
    ENEMY_GOBLIN,
    ENEMY_SKELETON,
    ENEMY_WRAITH
} EnemyType;

// Item types for power‑ups
typedef enum {
    ITEM_HEALTH,
    ITEM_BOMB,
    ITEM_FREEZE
} ItemType;

// Data structure for the player
typedef struct {
    int x, y;          // position on the dungeon grid
    int health;        // current health
    int maxHealth;     // maximum health including upgrades
    int score;         // accumulated score
    int combo;         // current combo multiplier
    int wave;          // current wave on this floor
} Player;

// Data structure for a normal enemy
typedef struct {
    int x, y;              // position
    char word[32];         // word to type to defeat
    bool active;           // active flag
    float moveTimer;       // timer to control movement
    float moveDelay;       // delay between moves
    EnemyType type;        // type of enemy
    Color color;           // colour for drawing
    int points;            // points awarded when defeated
} Enemy;

// Data structure for collectible items
typedef struct {
    int x, y;          // position
    ItemType type;     // item type
    char word[32];     // word to type to collect
    bool active;       // active flag
} Item;

// Data structure for traps
typedef struct {
    int x, y;          // position
    bool active;       // trap exists in the dungeon
    bool triggered;    // player stepped on trap and needs to disarm
    float timer;       // countdown timer for disarming
} Trap;

// Data structure for boss encounters
typedef struct {
    int x, y;          // position
    int hp;            // number of words remaining
    char word[32];     // current word to type
    float moveTimer;   // timer to control movement
    float moveDelay;   // boss movement delay
    bool active;       // boss is present
} Boss;

// Particle effect for impacts and item pickups
typedef struct {
    Vector2 pos;
    Vector2 vel;
    float life;
    float maxLife;
    Color color;
    bool active;
} Particle;

// Floating text for displaying messages and scores
typedef struct {
    Vector2 pos;
    char text[32];
    float life;
    Color color;
    bool active;
} FloatingText;

// Global game objects
static int dungeon[MAP_HEIGHT][MAP_WIDTH];
static Player player;
static Enemy enemies[MAX_ENEMIES];
static Item items[MAX_ITEMS];
static Trap traps[MAX_TRAPS];
static Boss boss;
static Particle particles[MAX_PARTICLES];
static FloatingText floatingTexts[MAX_FLOATING_TEXTS];

// Typing and timing state
static char typedInput[64] = "";
static float spawnTimer = 0.0f;
static float itemSpawnTimer = 0.0f;
static int activeEnemies = 0;
static float shakeTimer = 0.0f;
static float shakeStrength = 0.0f;
static bool gameOver = false;
static GameState gameState = STATE_MENU;
static bool inBossFight = false;
static float freezeTimer = 0.0f;

// Fog of war: track which tiles have been visited.  A visited tile will
// remain visible even when the player moves away from it.  This adds
// exploration to the roguelike by hiding unexplored areas until the
// player gets close.  Unvisited tiles that are beyond a small radius
// around the player are drawn much darker in DrawGameWorld().
static bool visited[MAP_HEIGHT][MAP_WIDTH];

// Difficulty settings.  1 = Easy, 2 = Normal, 3 = Hard.  The
// difficulty affects enemy spawn rates and movement speeds and word
// selection thresholds.  Players can cycle the difficulty on the
// menu screen by pressing the D key.  The default is Normal.
static int difficulty = 2;
static const char *difficultyNames[] = {"", "Easy", "Normal", "Hard"};

// Progression and economy
static int floorLevel = 1;
static int playerGold = 0;
static int highScore = 0;

// Persistent upgrades
static int upgradeMaxHealth = 0;
static float upgradeMoveSpeed = 1.0f;
static float upgradeEnemySlow = 1.0f;
static int upgradeComboBonus = 0;

// Flags for high score updates
static bool newHighAchieved = false;
static bool highUpdated = false;

// Sound effects
static Sound sfxKill;
static Sound sfxMiss;
static Sound sfxDamage;
static Sound sfxItem;
static Sound sfxBomb;
static Sound sfxFreeze;

// Word bank for enemy and item words.  We store all words in a fixed
// array of strings; new words can be loaded from an external file.
#define MAX_WORD_BANK 600
static char wordBank[MAX_WORD_BANK][32];
static int wordCount = 0;

// -----------------------------------------------------------------------------
// Forward declarations
static void InitWordBank(void);
static void LoadExternalWords(void);
static const char *GetRandomWordForWave(int wave);
static const char *GetRandomLongWord(void);
static void LoadSaveData(void);
static void SaveSaveData(void);
static void ResetGame(void);
static void GenerateDungeon(void);
static bool IsFloor(int x, int y);
static void PlacePlayer(void);
static void SpawnEnemy(void);
static void RemoveEnemy(int index);
static void SpawnItem(void);
static void PickupItem(ItemType type);
static void UpdateItems(float dt);
static void SpawnTrap(void);
static void UpdateTraps(float dt);
static void HandleTrapTyping(void);
static void SpawnBoss(void);
static void UpdateBoss(float dt);
static void SpawnParticles(float x, float y, Color color);
static void AddFloatingText(float x, float y, const char *text, Color color);
static void UpdateParticles(float dt);
static void UpdateFloatingTexts(float dt);
static void ClearEnemies(void);
static void NextFloor(void);
static void EnterShop(void);
static void HandleShopInput(void);
static void DrawGameWorld(void);
static void DrawHUD(void);
static void DrawMenuScreen(void);
static void DrawShopScreen(void);
static Sound GenerateTone(float frequency, float duration, float volume);
static void InitSoundEffects(void);
static void UnloadSoundEffects(void);

// -----------------------------------------------------------------------------
// Utility functions

// Initialise the base word list used for enemies and items.  Feel free to
// extend this list; additional words can also be loaded from an external
// wordlist.txt file via LoadExternalWords().
static void InitWordBank(void) {
    const char *baseWords[] = {
        // Easy words
        "cat", "map", "run", "gold", "dark", "door", "loot", "fire",
        "spell", "sword", "ghost", "crypt", "arrow", "torch", "key", "lock",
        "orc", "elf", "kobold", "coin", "pit", "trap",
        // Medium words
        "python", "typing", "wizard", "dungeon", "monster", "shadow",
        "potion", "skeleton", "treasure", "portal", "castle", "dagger",
        "forest", "scroll", "magic", "keyboard", "adventure", "explore",
        // Harder words
        "roguelike", "labyrinth", "necromancer", "basilisk", "sorcerer",
        "enchantment", "catacombs", "rhapsody", "phantasm", "ethereal"
    };
    int count = sizeof(baseWords) / sizeof(baseWords[0]);
    for (int i = 0; i < count && wordCount < MAX_WORD_BANK; i++) {
        strncpy(wordBank[wordCount], baseWords[i], sizeof(wordBank[wordCount]) - 1);
        wordBank[wordCount][sizeof(wordBank[wordCount]) - 1] = '\0';
        wordCount++;
    }
}

// Load additional words from wordlist.txt, one per line.  Words longer than
// 31 characters are truncated.  Lines beginning with # are treated as
// comments.
static void LoadExternalWords(void) {
    FILE *file = fopen("wordlist.txt", "r");
    if (!file) return;
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), file) && wordCount < MAX_WORD_BANK) {
        // Skip comments and empty lines
        if (buffer[0] == '#' || buffer[0] == '\n' || buffer[0] == '\0') continue;
        // Strip newline
        size_t len = strlen(buffer);
        if (buffer[len - 1] == '\n') buffer[len - 1] = '\0';
        // Copy word
        strncpy(wordBank[wordCount], buffer, sizeof(wordBank[wordCount]) - 1);
        wordBank[wordCount][sizeof(wordBank[wordCount]) - 1] = '\0';
        wordCount++;
    }
    fclose(file);
}

// Retrieve a random word appropriate for the current wave.  Earlier waves
// favour shorter words, while later waves may include longer ones.
static const char *GetRandomWordForWave(int wave) {
    // Adjust the effective wave based on difficulty.  Easier difficulty
    // reduces the wave level so shorter words remain available longer;
    // harder difficulty increases it so longer words appear sooner.
    int effectiveWave = wave;
    if (difficulty == 1 && effectiveWave > 1) effectiveWave--;
    else if (difficulty == 3) effectiveWave++;
    int attempts = 0;
    while (attempts < 40) {
        int index = rand() % wordCount;
        const char *w = wordBank[index];
        size_t len = strlen(w);
        if (effectiveWave < 3) {
            if (len <= 5) return w;
        } else if (effectiveWave < 6) {
            if (len <= 8) return w;
        } else {
            if (len > 4) return w;
        }
        attempts++;
    }
    return wordBank[rand() % wordCount];
}

// Retrieve a random longer word for boss battles.  Chooses from words with
// length >= 8.  If none available, returns a random word.
static const char *GetRandomLongWord(void) {
    int attempts = 0;
    while (attempts < 50) {
        int index = rand() % wordCount;
        const char *w = wordBank[index];
        if (strlen(w) >= 8) return w;
        attempts++;
    }
    return wordBank[rand() % wordCount];
}

// Load save data from save.dat.  The file stores high score, gold and
// upgrade levels.  If not present or malformed, defaults are used.
static void LoadSaveData(void) {
    FILE *file = fopen("save.dat", "r");
    if (!file) return;
    int hs = 0;
    int gold = 0;
    int maxH = 0;
    float moveUp = 1.0f;
    float slowUp = 1.0f;
    int comboUp = 0;
    if (fscanf(file, "%d %d %d %f %f %d", &hs, &gold, &maxH, &moveUp, &slowUp, &comboUp) == 6) {
        highScore = hs;
        playerGold = gold;
        upgradeMaxHealth = maxH;
        upgradeMoveSpeed = moveUp;
        upgradeEnemySlow = slowUp;
        upgradeComboBonus = comboUp;
    }
    fclose(file);
}

// Save save data to save.dat.  Called whenever upgrades change or a new high
// score is achieved.
static void SaveSaveData(void) {
    FILE *file = fopen("save.dat", "w");
    if (!file) return;
    fprintf(file, "%d %d %d %.3f %.3f %d\n", highScore, playerGold, upgradeMaxHealth,
            upgradeMoveSpeed, upgradeEnemySlow, upgradeComboBonus);
    fclose(file);
#ifdef PLATFORM_WEB
    EM_ASM({
        try {
            localStorage.setItem('dungeonKeys.save', FS.readFile('save.dat', { encoding: 'utf8' }));
        } catch (_) { /* The run remains playable without persistent storage. */ }
    });
#endif
}

// Reset the entire game.  Called when starting or restarting.  It does
// not reset persistent upgrades or gold, but it resets the dungeon,
// player, enemies, items, traps, boss and related timers.
static void ResetGame(void) {
    GenerateDungeon();
    PlacePlayer();
    // Reset fog of war: mark all tiles as unvisited and mark the
    // player's starting position as visited.  The visited array is
    // persisted between floors, so each ResetGame() call should
    // clear it for a fresh start.
    for (int yy = 0; yy < MAP_HEIGHT; yy++) {
        for (int xx = 0; xx < MAP_WIDTH; xx++) {
            visited[yy][xx] = false;
        }
    }
    visited[player.y][player.x] = true;
    player.health = 5 + upgradeMaxHealth;
    player.maxHealth = 5 + upgradeMaxHealth;
    player.score = 0;
    player.combo = 0;
    player.wave = 1;
    activeEnemies = 0;
    spawnTimer = 0.0f;
    itemSpawnTimer = 0.0f;
    shakeTimer = 0.0f;
    shakeStrength = 0.0f;
    freezeTimer = 0.0f;
    gameOver = false;
    inBossFight = false;
    // Reset enemies
    for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = false;
    // Reset items
    for (int i = 0; i < MAX_ITEMS; i++) items[i].active = false;
    // Reset traps
    for (int i = 0; i < MAX_TRAPS; i++) {
        traps[i].active = false;
        traps[i].triggered = false;
        traps[i].timer = 0.0f;
    }
    // Reset boss
    boss.active = false;
    boss.hp = 0;
    // Spawn some initial enemies and traps
    for (int i = 0; i < 3; i++) SpawnEnemy();
    for (int i = 0; i < MAX_TRAPS; i++) SpawnTrap();
}

// Generate a new dungeon layout.  Fills the map with walls then carves
// random rooms and connecting corridors.  Rooms are not stored separately.
static void GenerateDungeon(void) {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            dungeon[y][x] = 1;
        }
    }
    // Carve rooms
    int roomCount = 9;
    for (int r = 0; r < roomCount; r++) {
        int roomW = 4 + rand() % 6;
        int roomH = 3 + rand() % 5;
        int roomX = 1 + rand() % (MAP_WIDTH - roomW - 2);
        int roomY = 1 + rand() % (MAP_HEIGHT - roomH - 2);
        for (int y = roomY; y < roomY + roomH; y++) {
            for (int x = roomX; x < roomX + roomW; x++) {
                dungeon[y][x] = 0;
            }
        }
        // Connect to centre
        if (r > 0) {
            int cx = roomX + roomW / 2;
            int cy = roomY + roomH / 2;
            int targetX = MAP_WIDTH / 2;
            int targetY = MAP_HEIGHT / 2;
            int startX = cx < targetX ? cx : targetX;
            int endX   = cx > targetX ? cx : targetX;
            for (int x = startX; x <= endX; x++) dungeon[cy][x] = 0;
            int startY = cy < targetY ? cy : targetY;
            int endY   = cy > targetY ? cy : targetY;
            for (int y = startY; y <= endY; y++) dungeon[y][targetX] = 0;
        }
    }
    // Random holes
    for (int y = 1; y < MAP_HEIGHT - 1; y++) {
        for (int x = 1; x < MAP_WIDTH - 1; x++) {
            if (rand() % 100 < 4) dungeon[y][x] = 0;
        }
    }
}

// Check if a tile is walkable floor
static bool IsFloor(int x, int y) {
    return (x >= 0 && y >= 0 && x < MAP_WIDTH && y < MAP_HEIGHT && dungeon[y][x] == 0);
}

// Place the player on a random floor tile
static void PlacePlayer(void) {
    while (true) {
        int x = rand() % MAP_WIDTH;
        int y = rand() % MAP_HEIGHT;
        if (IsFloor(x, y)) {
            player.x = x;
            player.y = y;
            return;
        }
    }
}

// Spawn a normal enemy on a random floor tile away from the player.  The
// enemy type and stats depend on the current wave and upgrades.
static void SpawnEnemy(void) {
    if (activeEnemies >= MAX_ENEMIES) return;
    int slot = -1;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) { slot = i; break; }
    }
    if (slot == -1) return;
    // Choose position
    for (int attempt = 0; attempt < 120; attempt++) {
        int x = rand() % MAP_WIDTH;
        int y = rand() % MAP_HEIGHT;
        if (!IsFloor(x, y)) continue;
        int distance = abs(x - player.x) + abs(y - player.y);
        if (distance < 7) continue;
        enemies[slot].x = x;
        enemies[slot].y = y;
        enemies[slot].active = true;
        // Choose type based on wave
        int roll = rand() % 100;
        EnemyType type;
        if (player.wave < 3) {
            type = ENEMY_GOBLIN;
        } else if (player.wave < 6) {
            type = (roll < 65) ? ENEMY_GOBLIN : ENEMY_SKELETON;
        } else {
            if (roll < 45) type = ENEMY_GOBLIN;
            else if (roll < 80) type = ENEMY_SKELETON;
            else type = ENEMY_WRAITH;
        }
        enemies[slot].type = type;
        // Difficulty factor for enemy movement speed: easy slows enemies, hard speeds them up
        float diffDelayMult = (difficulty == 1 ? 1.1f : (difficulty == 3 ? 0.9f : 1.0f));
        switch (type) {
            case ENEMY_GOBLIN:
                enemies[slot].moveDelay = BASE_ENEMY_DELAY_GOBLIN * upgradeEnemySlow * diffDelayMult;
                enemies[slot].points    = 10 + upgradeComboBonus;
                enemies[slot].color     = (Color){90, 220, 110, 255};
                break;
            case ENEMY_SKELETON:
                enemies[slot].moveDelay = BASE_ENEMY_DELAY_SKELETON * upgradeEnemySlow * diffDelayMult;
                enemies[slot].points    = 16 + upgradeComboBonus;
                enemies[slot].color     = (Color){230, 230, 210, 255};
                break;
            case ENEMY_WRAITH:
            default:
                enemies[slot].moveDelay = BASE_ENEMY_DELAY_WRAITH * upgradeEnemySlow * diffDelayMult;
                enemies[slot].points    = 25 + upgradeComboBonus;
                enemies[slot].color     = (Color){180, 90, 255, 255};
                break;
        }
        enemies[slot].moveTimer = 0.0f;
        strncpy(enemies[slot].word, GetRandomWordForWave(player.wave), sizeof(enemies[slot].word) - 1);
        enemies[slot].word[sizeof(enemies[slot].word) - 1] = '\0';
        activeEnemies++;
        return;
    }
}

// Remove an enemy and award points and gold.  Trigger particle effects and
// floating text.  Also handle wave progression and boss spawning.
static void RemoveEnemy(int index) {
    if (index < 0 || index >= MAX_ENEMIES) return;
    if (!enemies[index].active) return;
    float px = enemies[index].x * TILE_SIZE + TILE_SIZE / 2.0f;
    float py = enemies[index].y * TILE_SIZE + TILE_SIZE / 2.0f;
    // Spawn particles
    SpawnParticles(px, py, enemies[index].color);
    // Award score and gold (gold equals points here)
    int gained = enemies[index].points + player.combo * 2;
    player.score += gained;
    playerGold += gained;
    player.combo++;
    char text[32];
    snprintf(text, sizeof(text), "+%d", gained);
    AddFloatingText(px - 10, py - 20, text, GOLD);
    enemies[index].active = false;
    activeEnemies--;
    PlaySound(sfxKill);
    // Increase wave every 8 defeated enemies
    if ((player.score / 80) + 1 > player.wave) {
        player.wave++;
        AddFloatingText(SCREEN_WIDTH / 2 - 45, 60, "WAVE UP!", SKYBLUE);
        // Boss spawns when wave reaches multiple of WAVES_PER_FLOOR
        if ((player.wave % WAVES_PER_FLOOR == 0) && !boss.active) {
            SpawnBoss();
        }
    }
}

// Spawn a collectible item randomly on a floor tile.  Items have their own
// words; when typed they activate an effect and disappear.
static void SpawnItem(void) {
    // 50% chance to spawn at each interval
    if ((rand() % 100) >= 50) return;
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].active) {
            // Find a position far from player
            for (int attempt = 0; attempt < 100; attempt++) {
                int x = rand() % MAP_WIDTH;
                int y = rand() % MAP_HEIGHT;
                if (!IsFloor(x, y)) continue;
                int dist = abs(x - player.x) + abs(y - player.y);
                if (dist < 5) continue;
                items[i].x = x;
                items[i].y = y;
                items[i].active = true;
                // Random item type
                int r = rand() % 3;
                items[i].type = (ItemType)r;
                switch (items[i].type) {
                    case ITEM_HEALTH:
                        strncpy(items[i].word, "heal", sizeof(items[i].word) - 1);
                        break;
                    case ITEM_BOMB:
                        strncpy(items[i].word, "bomb", sizeof(items[i].word) - 1);
                        break;
                    case ITEM_FREEZE:
                    default:
                        strncpy(items[i].word, "freeze", sizeof(items[i].word) - 1);
                        break;
                }
                items[i].word[sizeof(items[i].word) - 1] = '\0';
                return;
            }
        }
    }
}

// Apply the effect of a collected item
static void PickupItem(ItemType type) {
    switch (type) {
        case ITEM_HEALTH:
            if (player.health < player.maxHealth) {
                player.health++;
            }
            AddFloatingText(player.x * TILE_SIZE, player.y * TILE_SIZE - 20, "+HP", GREEN);
            PlaySound(sfxItem);
            break;
        case ITEM_BOMB:
            ClearEnemies();
            AddFloatingText(player.x * TILE_SIZE, player.y * TILE_SIZE - 20, "BOOM!", ORANGE);
            PlaySound(sfxBomb);
            break;
        case ITEM_FREEZE:
            freezeTimer = 4.0f;
            AddFloatingText(player.x * TILE_SIZE, player.y * TILE_SIZE - 20, "FROZEN", SKYBLUE);
            PlaySound(sfxFreeze);
            break;
    }
}

// Update active items and check if their words have been typed
static void UpdateItems(float dt) {
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].active) continue;
        int typedLength = (int)strlen(typedInput);
        // Check prefix match
        if (typedLength > 0 && strncmp(items[i].word, typedInput, typedLength) == 0) {
            if (strcmp(items[i].word, typedInput) == 0) {
                // Full match: collect item
                PickupItem(items[i].type);
                items[i].active = false;
                typedInput[0] = '\0';
                // Remove item from display
                continue;
            }
        }
    }
}

// Spawn a trap in a random location.  Traps remain inactive until
// triggered when the player steps on them.
static void SpawnTrap(void) {
    for (int i = 0; i < MAX_TRAPS; i++) {
        if (!traps[i].active) {
            for (int attempt = 0; attempt < 80; attempt++) {
                int x = rand() % MAP_WIDTH;
                int y = rand() % MAP_HEIGHT;
                if (!IsFloor(x, y)) continue;
                // Do not place trap on player start
                if (x == player.x && y == player.y) continue;
                // Place trap
                traps[i].x = x;
                traps[i].y = y;
                traps[i].active = true;
                traps[i].triggered = false;
                traps[i].timer = 0.0f;
                return;
            }
        }
    }
}

// Update traps: if triggered, count down and cause damage if not disarmed.
static void UpdateTraps(float dt) {
    for (int i = 0; i < MAX_TRAPS; i++) {
        if (!traps[i].active) continue;
        // Trigger if player steps on trap
        if (!traps[i].triggered && traps[i].x == player.x && traps[i].y == player.y) {
            traps[i].triggered = true;
            traps[i].timer = 3.0f; // seconds to disarm
            AddFloatingText(player.x * TILE_SIZE, player.y * TILE_SIZE - 20, "TRAP!", RED);
        }
        // If triggered, countdown
        if (traps[i].triggered) {
            traps[i].timer -= dt;
            if (traps[i].timer <= 0.0f) {
                // If not disarmed, hurt player
                player.health--;
                player.combo = 0;
                AddFloatingText(player.x * TILE_SIZE, player.y * TILE_SIZE - 20, "OUCH", RED);
                SpawnParticles(player.x * TILE_SIZE + TILE_SIZE / 2.0f, player.y * TILE_SIZE + TILE_SIZE / 2.0f, RED);
                traps[i].active = false;
                traps[i].triggered = false;
                if (player.health <= 0) {
                    gameOver = true;
                    typedInput[0] = '\0';
                }
            }
        }
    }
}

// Check typed input for trap disarming.  If the word "disarm" is fully
// typed while a trap is triggered, the trap is removed without damage.
static void HandleTrapTyping(void) {
    int typedLength = (int)strlen(typedInput);
    if (typedLength == 0) return;
    // Only consider traps that are triggered
    for (int i = 0; i < MAX_TRAPS; i++) {
        if (traps[i].active && traps[i].triggered) {
            const char *target = "disarm";
            if (strncmp(target, typedInput, typedLength) == 0) {
                if (strcmp(target, typedInput) == 0) {
                    traps[i].active = false;
                    traps[i].triggered = false;
                    traps[i].timer = 0.0f;
                    AddFloatingText(player.x * TILE_SIZE, player.y * TILE_SIZE - 20, "SAFE", GREEN);
                    typedInput[0] = '\0';
                }
                return;
            }
        }
    }
}

// Spawn a boss on the current floor.  Bosses require multiple long words
// to defeat.  They move slowly and periodically summon enemies.
static void SpawnBoss(void) {
    // Place boss far from player
    for (int attempt = 0; attempt < 100; attempt++) {
        int x = rand() % MAP_WIDTH;
        int y = rand() % MAP_HEIGHT;
        if (!IsFloor(x, y)) continue;
        int dist = abs(x - player.x) + abs(y - player.y);
        if (dist < 10) continue;
        boss.x = x;
        boss.y = y;
        boss.hp = BOSS_WORDS + floorLevel - 1; // Boss gets tougher on higher floors
        strncpy(boss.word, GetRandomLongWord(), sizeof(boss.word) - 1);
        boss.word[sizeof(boss.word) - 1] = '\0';
        boss.moveDelay = 1.0f * upgradeEnemySlow;
        boss.moveTimer = 0.0f;
        boss.active = true;
        inBossFight = true;
        AddFloatingText(SCREEN_WIDTH / 2 - 70, 80, "BOSS ARRIVES", PURPLE);
        return;
    }
}

// Update the boss: move towards player and check for defeat.  During the
// fight the boss occasionally summons new enemies.
static void UpdateBoss(float dt) {
    if (!boss.active) return;
    // Move boss occasionally
    boss.moveTimer += dt;
    if (boss.moveTimer >= boss.moveDelay) {
        boss.moveTimer = 0.0f;
        int dx = player.x - boss.x;
        int dy = player.y - boss.y;
        int stepX = 0;
        int stepY = 0;
        if (abs(dx) > abs(dy)) stepX = dx > 0 ? 1 : -1;
        else stepY = dy > 0 ? 1 : -1;
        int newX = boss.x + stepX;
        int newY = boss.y + stepY;
        if (IsFloor(newX, newY)) {
            boss.x = newX;
            boss.y = newY;
        }
    }
    // Collision with player
    if (boss.x == player.x && boss.y == player.y) {
        player.health--;
        player.combo = 0;
        AddFloatingText(player.x * TILE_SIZE, player.y * TILE_SIZE - 20, "-HP", RED);
        SpawnParticles(player.x * TILE_SIZE + TILE_SIZE / 2.0f, player.y * TILE_SIZE + TILE_SIZE / 2.0f, RED);
        PlaySound(sfxDamage);
        if (player.health <= 0) {
            gameOver = true;
            typedInput[0] = '\0';
        }
    }
    // Summon enemies occasionally
    if (((int)(boss.moveTimer * 10) % 7) == 0) {
        if (rand() % 100 < 10) {
            SpawnEnemy();
        }
    }
    // Boss word typing check
    int typedLength = (int)strlen(typedInput);
    if (typedLength > 0 && strncmp(boss.word, typedInput, typedLength) == 0) {
        if (strcmp(boss.word, typedInput) == 0) {
            boss.hp--;
            typedInput[0] = '\0';
            AddFloatingText(boss.x * TILE_SIZE, boss.y * TILE_SIZE - 20, "HIT", YELLOW);
            PlaySound(sfxKill);
            if (boss.hp > 0) {
                strncpy(boss.word, GetRandomLongWord(), sizeof(boss.word) - 1);
                boss.word[sizeof(boss.word) - 1] = '\0';
            } else {
                // Boss defeated
                boss.active = false;
                inBossFight = false;
                AddFloatingText(SCREEN_WIDTH/2 - 60, 60, "BOSS DOWN", PURPLE);
                // Award bonus gold and score
                int bonus = 100 + 50 * floorLevel;
                player.score += bonus;
                playerGold += bonus;
                char buf[32];
                snprintf(buf, sizeof(buf), "+%d", bonus);
                AddFloatingText(boss.x * TILE_SIZE, boss.y * TILE_SIZE - 40, buf, GOLD);
                // Proceed to next floor via shop
                EnterShop();
            }
        }
    }
}

// Clear all active enemies.  Used by bombs and between floors.
static void ClearEnemies(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = false;
    }
    activeEnemies = 0;
}

// Move to the next floor.  Increments the floor level, resets the
// dungeon and player wave, and spawns new enemies and traps.  The
// player keeps their health, score, combo and gold.
static void NextFloor(void) {
    floorLevel++;
    player.wave = 1;
    GenerateDungeon();
    PlacePlayer();
    // Reset visited tiles for the new floor and mark the player's
    // starting tile as visited.  The fog of war is floor‑specific.
    for (int yy = 0; yy < MAP_HEIGHT; yy++) {
        for (int xx = 0; xx < MAP_WIDTH; xx++) {
            visited[yy][xx] = false;
        }
    }
    visited[player.y][player.x] = true;
    // Clear entities
    ClearEnemies();
    for (int i = 0; i < MAX_ITEMS; i++) items[i].active = false;
    for (int i = 0; i < MAX_TRAPS; i++) {
        traps[i].active = false;
        traps[i].triggered = false;
        traps[i].timer = 0.0f;
    }
    boss.active = false;
    inBossFight = false;
    freezeTimer = 0.0f;
    spawnTimer = 0.0f;
    itemSpawnTimer = 0.0f;
    // Spawn initial enemies and traps
    for (int i = 0; i < 3 + floorLevel; i++) SpawnEnemy();
    for (int i = 0; i < MAX_TRAPS; i++) SpawnTrap();
    // Show floor text
    char buf[32];
    snprintf(buf, sizeof(buf), "Floor %d", floorLevel);
    AddFloatingText(SCREEN_WIDTH/2 - 40, 90, buf, SKYBLUE);
}

// Enter the shop state.  During the shop the player can spend gold to
// purchase permanent upgrades.  After leaving the shop, the next floor
// begins.
static void EnterShop(void) {
    gameState = STATE_SHOP;
}

// Handle keyboard input during the shop.  Purchasing upgrades deducts
// gold and increases the corresponding upgrade value.  The cost does
// not increase with each purchase for simplicity.  Pressing Enter or
// Escape exits the shop and moves to the next floor.
static void HandleShopInput(void) {
    // Numbers 1–4 correspond to upgrade options
    if (IsKeyPressed(KEY_ONE)) {
        if (playerGold >= COST_UPGRADE_HEALTH) {
            playerGold -= COST_UPGRADE_HEALTH;
            upgradeMaxHealth++;
            player.maxHealth++;
            player.health++;
            AddFloatingText(SCREEN_WIDTH/2 - 60, 120, "+1 MAX HP", GREEN);
            PlaySound(sfxItem);
            SaveSaveData();
        }
    } else if (IsKeyPressed(KEY_TWO)) {
        if (playerGold >= COST_UPGRADE_MOVES) {
            playerGold -= COST_UPGRADE_MOVES;
            upgradeMoveSpeed *= 0.9f;
            AddFloatingText(SCREEN_WIDTH/2 - 70, 120, "SPEED UP", GREEN);
            PlaySound(sfxItem);
            SaveSaveData();
        }
    } else if (IsKeyPressed(KEY_THREE)) {
        if (playerGold >= COST_UPGRADE_SLOW) {
            playerGold -= COST_UPGRADE_SLOW;
            upgradeEnemySlow *= 1.1f;
            AddFloatingText(SCREEN_WIDTH/2 - 80, 120, "ENEMIES SLOWER", GREEN);
            PlaySound(sfxItem);
            SaveSaveData();
        }
    } else if (IsKeyPressed(KEY_FOUR)) {
        if (playerGold >= COST_UPGRADE_COMBO) {
            playerGold -= COST_UPGRADE_COMBO;
            upgradeComboBonus++;
            AddFloatingText(SCREEN_WIDTH/2 - 80, 120, "+COMBO BONUS", GREEN);
            PlaySound(sfxItem);
            SaveSaveData();
        }
    }
    // Exit shop
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
        // Start next floor
        gameState = STATE_PLAY;
        NextFloor();
    }
}

// Draw the dungeon, player, enemies, items, traps, boss, particles and
// floating texts.  Applies screen shake offsets and freeze tint when
// necessary.
static void DrawGameWorld(void) {
    // Shake offset
    Vector2 shake = {0, 0};
    if (shakeTimer > 0.0f) {
        shake.x = ((float)(rand() % 100 - 50) / 50.0f) * shakeStrength;
        shake.y = ((float)(rand() % 100 - 50) / 50.0f) * shakeStrength;
    }
    BeginMode2D((Camera2D){ .offset = shake, .target = (Vector2){0, 0}, .rotation = 0, .zoom = 1.0f });
    // Draw dungeon tiles
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            Rectangle tile = { x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE };
            // Determine if this tile should be darkened.  A tile is dark
            // if it has never been visited and is beyond a small radius
            // around the player.  This creates a fog‑of‑war effect.
            bool dark = !visited[y][x] && ((abs(x - player.x) + abs(y - player.y)) > 3);
            if (dungeon[y][x] == 1) {
                Color wallCol = (Color){36, 40, 55, 255};
                Color wallLine = (Color){16, 18, 26, 255};
                if (dark) {
                    wallCol = (Color){20, 24, 32, 255};
                    wallLine = (Color){10, 12, 18, 255};
                }
                DrawRectangleRec(tile, wallCol);
                DrawRectangleLines(tile.x, tile.y, TILE_SIZE, TILE_SIZE, wallLine);
            } else {
                Color base = ((x + y) % 2 == 0) ? (Color){18, 22, 34, 255} : (Color){22, 26, 40, 255};
                Color lines = (Color){14, 16, 24, 255};
                if (dark) {
                    // Darken the floor tiles when unexplored
                    base = (Color){10, 12, 18, 255};
                    lines = (Color){8, 9, 14, 255};
                }
                DrawRectangleRec(tile, base);
                DrawRectangleLines(tile.x, tile.y, TILE_SIZE, TILE_SIZE, lines);
            }
        }
    }
    // Draw particles under characters
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) continue;
        float ratio = particles[i].life / particles[i].maxLife;
        Color c = Fade(particles[i].color, ratio);
        DrawCircleV(particles[i].pos, 3.0f * ratio, c);
    }
    // Draw player
    float px = player.x * TILE_SIZE + TILE_SIZE / 2.0f;
    float py = player.y * TILE_SIZE + TILE_SIZE / 2.0f;
    DrawCircleGradient((int)px, (int)py, 26, (Color){20, 120, 255, 120}, (Color){0, 0, 0, 0});
    DrawCircle((int)px, (int)py, 11, (Color){40, 165, 255, 255});
    DrawCircleLines((int)px, (int)py, 12, SKYBLUE);
    // Draw items
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].active) continue;
        float ix = items[i].x * TILE_SIZE + TILE_SIZE / 2.0f;
        float iy = items[i].y * TILE_SIZE + TILE_SIZE / 2.0f;
        Color col;
        if (items[i].type == ITEM_HEALTH) col = GREEN;
        else if (items[i].type == ITEM_BOMB) col = ORANGE;
        else col = SKYBLUE;
        DrawCircle((int)ix, (int)iy, 8, col);
        int wLen = MeasureText(items[i].word, 14);
        DrawText(items[i].word, (int)(ix - wLen / 2), (int)(iy - 28), 14, col);
    }
    // Draw traps (as red X)
    for (int i = 0; i < MAX_TRAPS; i++) {
        if (!traps[i].active) continue;
        float tx = traps[i].x * TILE_SIZE + TILE_SIZE / 2.0f;
        float ty = traps[i].y * TILE_SIZE + TILE_SIZE / 2.0f;
        DrawLine((int)(tx - 6), (int)(ty - 6), (int)(tx + 6), (int)(ty + 6), RED);
        DrawLine((int)(tx + 6), (int)(ty - 6), (int)(tx - 6), (int)(ty + 6), RED);
    }
    // Draw enemies
    int highlightedEnemy = -1;
    if (strlen(typedInput) > 0) {
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!enemies[i].active) continue;
            int typedLen = (int)strlen(typedInput);
            if (strncmp(enemies[i].word, typedInput, typedLen) == 0) {
                highlightedEnemy = i;
                break;
            }
        }
    }
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        float ex = enemies[i].x * TILE_SIZE + TILE_SIZE / 2.0f;
        float ey = enemies[i].y * TILE_SIZE + TILE_SIZE / 2.0f;
        DrawCircleGradient((int)ex, (int)ey, 24, Fade(enemies[i].color, 0.55f), (Color){0, 0, 0, 0});
        DrawCircle((int)ex, (int)ey, 11, enemies[i].color);
        if (i == highlightedEnemy) {
            DrawCircleLines((int)ex, (int)ey, 15, GOLD);
        }
        int wWidth = MeasureText(enemies[i].word, 16);
        int wx = (int)(ex - wWidth / 2);
        int wy = (int)(ey - 31);
        DrawRectangle(wx - 4, wy - 2, wWidth + 8, 20, (Color){0, 0, 0, 150});
        if (i == highlightedEnemy && strlen(typedInput) > 0) {
            int typedLen = (int)strlen(typedInput);
            char part[32];
            strncpy(part, enemies[i].word, typedLen);
            part[typedLen] = '\0';
            DrawText(part, wx, wy, 16, GOLD);
            DrawText(enemies[i].word + typedLen, wx + MeasureText(part, 16), wy, 16, RAYWHITE);
        } else {
            DrawText(enemies[i].word, wx, wy, 16, RAYWHITE);
        }
    }
    // Draw boss if active
    if (boss.active) {
        float bx = boss.x * TILE_SIZE + TILE_SIZE / 2.0f;
        float by = boss.y * TILE_SIZE + TILE_SIZE / 2.0f;
        // Boss body
        DrawCircleGradient((int)bx, (int)by, 30, (Color){100, 50, 200, 180}, (Color){0,0,0,0});
        DrawCircle((int)bx, (int)by, 14, (Color){140, 70, 240, 255});
        // Boss word
        int bw = MeasureText(boss.word, 18);
        DrawRectangle((int)(bx - bw/2 - 4), (int)(by - 38), bw + 8, 22, (Color){0,0,0,150});
        DrawText(boss.word, (int)(bx - bw/2), (int)(by - 36), 18, YELLOW);
        // Boss HP bar
        float barWidth = 60.0f;
        float hpRatio = (float)boss.hp / (float)(BOSS_WORDS + floorLevel - 1);
        DrawRectangle((int)(bx - barWidth/2), (int)(by + 22), (int)barWidth, 8, (Color){50,50,50,200});
        DrawRectangle((int)(bx - barWidth/2), (int)(by + 22), (int)(barWidth * hpRatio), 8, (Color){200, 60, 250, 220});
    }
    // Draw floating texts on top
    for (int i = 0; i < MAX_FLOATING_TEXTS; i++) {
        if (!floatingTexts[i].active) continue;
        Color c = Fade(floatingTexts[i].color, floatingTexts[i].life);
        DrawText(floatingTexts[i].text, (int)floatingTexts[i].pos.x, (int)floatingTexts[i].pos.y, 20, c);
    }
    EndMode2D();
}

// Draw the menu screen with controls and high score
static void DrawMenuScreen(void) {
    const char *title    = "Dungeon Keys";
    const char *subtitle = "Rogue Edition";
    const char *prompt   = "Press Enter or Space to begin";
    int tWidth  = MeasureText(title, 60);
    int stWidth = MeasureText(subtitle, 30);
    int pWidth  = MeasureText(prompt, 20);
    DrawText(title, SCREEN_WIDTH/2 - tWidth/2, SCREEN_HEIGHT/2 - 150, 60, SKYBLUE);
    DrawText(subtitle, SCREEN_WIDTH/2 - stWidth/2, SCREEN_HEIGHT/2 - 90, 30, LIGHTGRAY);
    DrawText(prompt, SCREEN_WIDTH/2 - pWidth/2, SCREEN_HEIGHT/2 - 30, 20, GRAY);
    char buf[80];
    snprintf(buf, sizeof(buf), "High Score: %d", highScore);
    int bWidth = MeasureText(buf, 22);
    DrawText(buf, SCREEN_WIDTH/2 - bWidth/2, SCREEN_HEIGHT/2 + 10, 22, LIGHTGRAY);
    // Display current difficulty and hint to change it
    char dBuf[64];
    snprintf(dBuf, sizeof(dBuf), "Difficulty: %s (Press D to change)", difficultyNames[difficulty]);
    int dWidth = MeasureText(dBuf, 18);
    DrawText(dBuf, SCREEN_WIDTH/2 - dWidth/2, SCREEN_HEIGHT/2 + 35, 18, DARKGRAY);
    const char *info1 = "Move with WASD/Arrow keys";
    const char *info2 = "Type enemy words to defeat them";
    const char *info3 = "Collect items by typing their words";
    DrawText(info1, SCREEN_WIDTH/2 - MeasureText(info1, 18)/2, SCREEN_HEIGHT/2 + 60, 18, DARKGRAY);
    DrawText(info2, SCREEN_WIDTH/2 - MeasureText(info2, 18)/2, SCREEN_HEIGHT/2 + 85, 18, DARKGRAY);
    DrawText(info3, SCREEN_WIDTH/2 - MeasureText(info3, 18)/2, SCREEN_HEIGHT/2 + 110, 18, DARKGRAY);
}

// Draw the HUD at the bottom of the screen.  Shows health, score, combo,
// wave, floor, gold, high score and input.  Also displays game over info.
static void DrawHUD(void) {
    Rectangle panel = { 0, MAP_HEIGHT * TILE_SIZE, SCREEN_WIDTH, STATUS_PANEL_HEIGHT };
    DrawRectangleRec(panel, (Color){12, 16, 26, 255});
    DrawRectangleLinesEx(panel, 2, (Color){35, 45, 65, 255});
    // Input display
    char inputBuf[128];
    snprintf(inputBuf, sizeof(inputBuf), "Input: %s", typedInput);
    DrawText(inputBuf, 18, MAP_HEIGHT * TILE_SIZE + 15, 24, RAYWHITE);
    // Status line 1
    char status1[256];
    snprintf(status1, sizeof(status1), "HP: %d/%d   Score: %d   Combo: x%d   Wave: %d   Floor: %d", player.health, player.maxHealth, player.score, player.combo, player.wave, floorLevel);
    DrawText(status1, 18, MAP_HEIGHT * TILE_SIZE + 50, 20, LIGHTGRAY);
    // Status line 2 with gold and high score
    char status2[256];
    snprintf(status2, sizeof(status2), "Gold: %d   High: %d", playerGold, highScore);
    DrawText(status2, 18, MAP_HEIGHT * TILE_SIZE + 75, 20, LIGHTGRAY);
    // Footer
    DrawText("Backspace deletes, Enter clears", 18, MAP_HEIGHT * TILE_SIZE + 100, 16, GRAY);
    // Draw hearts
    for (int i = 0; i < player.maxHealth; i++) {
        Color heartColor = i < player.health ? RED : (Color){80, 30, 40, 255};
        DrawCircle(SCREEN_WIDTH - 200 + i * 26, MAP_HEIGHT * TILE_SIZE + 35, 9, heartColor);
    }
    // Game over overlay
    if (gameState == STATE_GAME_OVER) {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){0, 0, 0, 170});
        const char *over = "GAME OVER";
        int ow = MeasureText(over, 64);
        DrawText(over, SCREEN_WIDTH/2 - ow/2, SCREEN_HEIGHT/2 - 120, 64, RED);
        // High score update once
        if (!highUpdated) {
            if (player.score > highScore) {
                highScore = player.score;
                newHighAchieved = true;
                SaveSaveData();
            }
            highUpdated = true;
        }
        // Show final score and high score
        char finalBuf[128];
        snprintf(finalBuf, sizeof(finalBuf), "Final Score: %d", player.score);
        int fw = MeasureText(finalBuf, 28);
        DrawText(finalBuf, SCREEN_WIDTH/2 - fw/2, SCREEN_HEIGHT/2 - 40, 28, RAYWHITE);
        char highBuf[128];
        snprintf(highBuf, sizeof(highBuf), "High Score: %d", highScore);
        int hw = MeasureText(highBuf, 22);
        DrawText(highBuf, SCREEN_WIDTH/2 - hw/2, SCREEN_HEIGHT/2 - 5, 22, LIGHTGRAY);
        if (newHighAchieved) {
            const char *nh = "NEW HIGH SCORE!";
            int nhw = MeasureText(nh, 26);
            DrawText(nh, SCREEN_WIDTH/2 - nhw/2, SCREEN_HEIGHT/2 - 75, 26, YELLOW);
        }
        const char *restart = "Press R to restart or M for menu";
        int rw = MeasureText(restart, 22);
        DrawText(restart, SCREEN_WIDTH/2 - rw/2, SCREEN_HEIGHT/2 + 40, 22, GOLD);
    }
    // Shop overlay
    if (gameState == STATE_SHOP) {
        DrawRectangle(0, MAP_HEIGHT * TILE_SIZE, SCREEN_WIDTH, STATUS_PANEL_HEIGHT, (Color){6, 10, 18, 240});
        DrawRectangleLines(0, MAP_HEIGHT * TILE_SIZE, SCREEN_WIDTH, STATUS_PANEL_HEIGHT, (Color){40, 50, 70, 255});
        const char *shopTitle = "SHOP – Spend your Gold";
        int stw = MeasureText(shopTitle, 28);
        DrawText(shopTitle, SCREEN_WIDTH/2 - stw/2, MAP_HEIGHT * TILE_SIZE + 10, 28, SKYBLUE);
        // Options
        char opt1[64], opt2[64], opt3[64], opt4[64];
        snprintf(opt1, sizeof(opt1), "1) +1 Max HP (%d gold)", COST_UPGRADE_HEALTH);
        snprintf(opt2, sizeof(opt2), "2) Faster Player (%d gold)", COST_UPGRADE_MOVES);
        snprintf(opt3, sizeof(opt3), "3) Slower Enemies (%d gold)", COST_UPGRADE_SLOW);
        snprintf(opt4, sizeof(opt4), "4) +Combo Bonus (%d gold)", COST_UPGRADE_COMBO);
        DrawText(opt1, 40, MAP_HEIGHT * TILE_SIZE + 50, 22, playerGold >= COST_UPGRADE_HEALTH ? GREEN : GRAY);
        DrawText(opt2, 40, MAP_HEIGHT * TILE_SIZE + 80, 22, playerGold >= COST_UPGRADE_MOVES ? GREEN : GRAY);
        DrawText(opt3, 40, MAP_HEIGHT * TILE_SIZE + 110, 22, playerGold >= COST_UPGRADE_SLOW ? GREEN : GRAY);
        DrawText(opt4, 40, MAP_HEIGHT * TILE_SIZE + 140, 22, playerGold >= COST_UPGRADE_COMBO ? GREEN : GRAY);
        const char *exitMsg = "Enter to continue to next floor";
        int emw = MeasureText(exitMsg, 20);
        DrawText(exitMsg, SCREEN_WIDTH/2 - emw/2, MAP_HEIGHT * TILE_SIZE + 170, 20, LIGHTGRAY);
    }
}

// Draw the shop screen.  This is handled inside DrawHUD since it shares
// the same panel.  The function remains for completeness but is unused.
static void DrawShopScreen(void) {
    // Shop draws inside DrawHUD
}

// Spawn particle effects at a given position.  Used for enemy deaths,
// damage and item pickups.
static void SpawnParticles(float x, float y, Color color) {
    for (int i = 0; i < 20; i++) {
        for (int p = 0; p < MAX_PARTICLES; p++) {
            if (!particles[p].active) {
                float angle = ((float)(rand() % 360)) * DEG2RAD;
                float speed = 70.0f + (float)(rand() % 100);
                particles[p].active = true;
                particles[p].pos = (Vector2){x, y};
                particles[p].vel = (Vector2){cosf(angle) * speed, sinf(angle) * speed};
                particles[p].life = 0.5f + (float)(rand() % 40) / 100.0f;
                particles[p].maxLife = particles[p].life;
                particles[p].color = color;
                break;
            }
        }
    }
}

// Add a floating text message.  Messages float upwards and fade out.
static void AddFloatingText(float x, float y, const char *text, Color color) {
    for (int i = 0; i < MAX_FLOATING_TEXTS; i++) {
        if (!floatingTexts[i].active) {
            floatingTexts[i].active = true;
            floatingTexts[i].pos = (Vector2){x, y};
            floatingTexts[i].life = 1.0f;
            floatingTexts[i].color = color;
            strncpy(floatingTexts[i].text, text, sizeof(floatingTexts[i].text) - 1);
            floatingTexts[i].text[sizeof(floatingTexts[i].text) - 1] = '\0';
            return;
        }
    }
}

// Update particle positions and fading
static void UpdateParticles(float dt) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) continue;
        particles[i].life -= dt;
        particles[i].pos.x += particles[i].vel.x * dt;
        particles[i].pos.y += particles[i].vel.y * dt;
        particles[i].vel.x *= 0.94f;
        particles[i].vel.y *= 0.94f;
        if (particles[i].life <= 0.0f) {
            particles[i].active = false;
        }
    }
}

// Update floating text positions and fading
static void UpdateFloatingTexts(float dt) {
    for (int i = 0; i < MAX_FLOATING_TEXTS; i++) {
        if (!floatingTexts[i].active) continue;
        floatingTexts[i].life -= dt;
        floatingTexts[i].pos.y -= 35.0f * dt;
        if (floatingTexts[i].life <= 0.0f) {
            floatingTexts[i].active = false;
        }
    }
}

// Generate simple tones for sound effects.  Additional effects like
// bombs and freeze scrolls use distinct frequencies.
static Sound GenerateTone(float frequency, float duration, float volume) {
    unsigned int sampleRate = 44100;
    unsigned int sampleCount = (unsigned int)(sampleRate * duration);
    float *buffer = (float *)MemAlloc(sampleCount * sizeof(float));
    for (unsigned int i = 0; i < sampleCount; i++) {
        float t = (float)i / (float)sampleRate;
        buffer[i] = sinf(2.0f * PI * frequency * t) * volume;
    }
    Wave wave = { .frameCount = sampleCount, .sampleRate = sampleRate, .sampleSize = 32, .channels = 1, .data = buffer };
    Sound sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return sound;
}

// Initialise sound effects
static void InitSoundEffects(void) {
    sfxKill   = GenerateTone(660.0f, 0.08f, 0.5f);
    sfxMiss   = GenerateTone(220.0f, 0.10f, 0.5f);
    sfxDamage = GenerateTone(120.0f, 0.15f, 0.6f);
    sfxItem   = GenerateTone(880.0f, 0.10f, 0.5f);
    sfxBomb   = GenerateTone(150.0f, 0.20f, 0.5f);
    sfxFreeze = GenerateTone(550.0f, 0.15f, 0.5f);
}

// Free sound resources
static void UnloadSoundEffects(void) {
    UnloadSound(sfxKill);
    UnloadSound(sfxMiss);
    UnloadSound(sfxDamage);
    UnloadSound(sfxItem);
    UnloadSound(sfxBomb);
    UnloadSound(sfxFreeze);
}

static void UpdateDrawFrame(void) {
    float dt = GetFrameTime();
    // Update based on game state
    switch (gameState) {
        case STATE_MENU:
            // Start the game with Enter/Space/N
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_N)) {
                ResetGame();
                gameState = STATE_PLAY;
            }
            // Cycle difficulty with D.  Difficulty persists until changed.
            if (IsKeyPressed(KEY_D)) {
                difficulty++;
                if (difficulty > 3) difficulty = 1;
                // Show a message indicating the new difficulty
                char buf[32];
                snprintf(buf, sizeof(buf), "Difficulty: %s", difficultyNames[difficulty]);
                AddFloatingText(SCREEN_WIDTH/2 - 80, SCREEN_HEIGHT/2 + 40, buf, GOLD);
            }
            break;
        case STATE_PLAY:
            if (!gameOver) {
                // Player movement
                if ((IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) && IsFloor(player.x - 1, player.y)) player.x--;
                if ((IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) && IsFloor(player.x + 1, player.y)) player.x++;
                if ((IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) && IsFloor(player.x, player.y - 1)) player.y--;
                if ((IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) && IsFloor(player.x, player.y + 1)) player.y++;
                // Mark visited cells for fog of war: mark the player's tile and its
                // immediate neighbours as visited whenever the player moves.
                if (player.y >= 0 && player.y < MAP_HEIGHT && player.x >= 0 && player.x < MAP_WIDTH) {
                    visited[player.y][player.x] = true;
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            int nx = player.x + dx;
                            int ny = player.y + dy;
                            if (ny >= 0 && ny < MAP_HEIGHT && nx >= 0 && nx < MAP_WIDTH) {
                                visited[ny][nx] = true;
                            }
                        }
                    }
                }
                // Handle typing
                int key = GetCharPressed();
                while (key > 0) {
                    if (((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z')) && strlen(typedInput) < sizeof(typedInput) - 1) {
                        char c = (char)key;
                        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
                        size_t len = strlen(typedInput);
                        typedInput[len] = c;
                        typedInput[len + 1] = '\0';
                        // Check enemy matches
                        int target = -1;
                        for (int i = 0; i < MAX_ENEMIES; i++) {
                            if (!enemies[i].active) continue;
                            if (strncmp(enemies[i].word, typedInput, strlen(typedInput)) == 0) {
                                target = i;
                                break;
                            }
                        }
                        if (target == -1) {
                            // Check items
                            bool matchedItem = false;
                            for (int j = 0; j < MAX_ITEMS; j++) {
                                if (!items[j].active) continue;
                                if (strncmp(items[j].word, typedInput, strlen(typedInput)) == 0) {
                                    matchedItem = true;
                                    break;
                                }
                            }
                            // Check trap disarm
                            bool trapMatch = false;
                            for (int t = 0; t < MAX_TRAPS; t++) {
                                if (traps[t].active && traps[t].triggered) {
                                    const char *tw = "disarm";
                                    if (strncmp(tw, typedInput, strlen(typedInput)) == 0) {
                                        trapMatch = true;
                                        break;
                                    }
                                }
                            }
                            if (!matchedItem && !trapMatch) {
                                // Miss
                                player.combo = 0;
                                typedInput[0] = '\0';
                                AddFloatingText(player.x * TILE_SIZE, player.y * TILE_SIZE - 20, "MISS", RED);
                                shakeTimer = 0.12f;
                                shakeStrength = 4.0f;
                                PlaySound(sfxMiss);
                            }
                        } else {
                            // If full word typed
                            if (strcmp(enemies[target].word, typedInput) == 0) {
                                RemoveEnemy(target);
                                typedInput[0] = '\0';
                            }
                        }
                    }
                    key = GetCharPressed();
                }
                // Handle backspace/enter
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    size_t l = strlen(typedInput);
                    if (l > 0) typedInput[l - 1] = '\0';
                }
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
                    typedInput[0] = '\0';
                }
                // Update traps disarming
                HandleTrapTyping();
                // Update timers
                // Calculate spawn rate factoring in upgrades and selected difficulty.  On
                // easy difficulty the spawn rate is longer (enemies spawn less often),
                // on hard difficulty it is shorter (more frequent spawns).  Upgrades
                // to player speed reduce the spawn rate further.
                float diffSpawnMult = (difficulty == 1 ? 1.3f : (difficulty == 3 ? 0.8f : 1.0f));
                float spawnRate = BASE_SPAWN_RATE * upgradeMoveSpeed * diffSpawnMult;
                spawnTimer += dt;
                if (!inBossFight && spawnTimer >= spawnRate) {
                    SpawnEnemy();
                    spawnTimer = 0.0f;
                }
                itemSpawnTimer += dt;
                if (itemSpawnTimer >= ITEM_SPAWN_INTERVAL) {
                    SpawnItem();
                    itemSpawnTimer = 0.0f;
                }
                // Freeze timer
                if (freezeTimer > 0.0f) {
                    freezeTimer -= dt;
                    if (freezeTimer < 0.0f) freezeTimer = 0.0f;
                }
                // Update enemies
                for (int i = 0; i < MAX_ENEMIES; i++) {
                    if (!enemies[i].active) continue;
                    // Move enemy if not frozen and if not boss
                    enemies[i].moveTimer += dt;
                    float delay = enemies[i].moveDelay;
                    if (freezeTimer > 0.0f) delay *= 2.0f;
                    if (enemies[i].moveTimer >= delay) {
                        enemies[i].moveTimer = 0.0f;
                        int dx = player.x - enemies[i].x;
                        int dy = player.y - enemies[i].y;
                        int stepX = 0;
                        int stepY = 0;
                        if (abs(dx) > abs(dy)) stepX = dx > 0 ? 1 : -1;
                        else stepY = dy > 0 ? 1 : -1;
                        int nx = enemies[i].x + stepX;
                        int ny = enemies[i].y + stepY;
                        if (IsFloor(nx, ny)) {
                            enemies[i].x = nx;
                            enemies[i].y = ny;
                        }
                    }
                    // Collision
                    if (enemies[i].x == player.x && enemies[i].y == player.y) {
                        player.health--;
                        player.combo = 0;
                        AddFloatingText(player.x * TILE_SIZE, player.y * TILE_SIZE - 20, "-HP", RED);
                        SpawnParticles(player.x * TILE_SIZE + TILE_SIZE / 2.0f, player.y * TILE_SIZE + TILE_SIZE / 2.0f, RED);
                        PlaySound(sfxDamage);
                        enemies[i].active = false;
                        activeEnemies--;
                        if (player.health <= 0) {
                            gameOver = true;
                            typedInput[0] = '\0';
                        }
                    }
                }
                // Update boss
                UpdateBoss(dt);
                // Update items and traps
                UpdateItems(dt);
                UpdateTraps(dt);
            } else {
                // If game over, handle restarts
                if (IsKeyPressed(KEY_R)) {
                    ResetGame();
                    gameState = STATE_PLAY;
                    newHighAchieved = false;
                    highUpdated = false;
                }
                if (IsKeyPressed(KEY_M)) {
                    ResetGame();
                    gameState = STATE_MENU;
                    newHighAchieved = false;
                    highUpdated = false;
                }
            }
            break;
        case STATE_SHOP:
            HandleShopInput();
            break;
        case STATE_GAME_OVER:
            if (IsKeyPressed(KEY_R)) {
                ResetGame();
                gameState = STATE_PLAY;
                newHighAchieved = false;
                highUpdated = false;
            }
            if (IsKeyPressed(KEY_M) || IsKeyPressed(KEY_ESCAPE)) {
                ResetGame();
                gameState = STATE_MENU;
                newHighAchieved = false;
                highUpdated = false;
            }
            break;
    }
    // Update particles and texts globally
    UpdateParticles(dt);
    UpdateFloatingTexts(dt);
    // Shake timer decays
    if (shakeTimer > 0.0f) {
        shakeTimer -= dt;
        if (shakeTimer < 0.0f) shakeTimer = 0.0f;
    }
    // Drawing
    BeginDrawing();
    ClearBackground((Color){8, 10, 18, 255});
    if (gameState == STATE_MENU) {
        DrawMenuScreen();
    } else {
        DrawGameWorld();
        DrawHUD();
    }
    EndDrawing();
}

int main(void) {
    srand((unsigned int)time(NULL));
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Dungeon Keys – Rogue Edition");
    InitAudioDevice();
    SetTargetFPS(60);
    SetMasterVolume(0.45f);
    // Initialise data
    InitWordBank();
    LoadExternalWords();
#ifdef PLATFORM_WEB
    // Browser storage keeps the existing save format without a server.
    EM_ASM({
        try {
            const saved = localStorage.getItem('dungeonKeys.save');
            if (saved) FS.writeFile('save.dat', saved);
        } catch (_) { /* Storage may be unavailable in private browsing. */ }
    });
#endif
    LoadSaveData();
    InitSoundEffects();
    ResetGame();
    gameState = STATE_MENU;
#ifdef PLATFORM_WEB
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    while (!WindowShouldClose()) UpdateDrawFrame();
#endif
    // Save on exit
    SaveSaveData();
    UnloadSoundEffects();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}