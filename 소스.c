#define _CRT_SECURE_NO_WARMNINGS
#include <stdio.h>
#include <stdbool.h>
#include <windows.h>
#include <conio.h>
#include <stdlib.h>   // system()

#define MAX_RESOURCES 100

#define BASE_INTERVAL 5.0
#define BASE_AMOUNT 25

#define PRODUCER_INTERVAL 4.0
#define PRODUCER_AMOUNT 25
#define ROWS 5
#define COLS 9
#define NO_UNIT (-1)

#define MAX_ENEMIES     100
#define MAX_PROJECTILES 200

#define SHOT_SPEED  6.0   /* 예시: 초당 6칸 이동 */
#define COMBAT_STEP 0.02  /* 전투를 0.02초 단위로 갱신 */


typedef enum {
    UNIT_ATTACK,
    UNIT_DEFENSE,
    UNIT_PRODUCER,
    UNIT_TYPE_COUNT
} UnitType;

/* 종류별 기본 정보 */
typedef struct {
    const char* name;

    int maxHp;                   // 기본 최대 체력
    int attackPower;             // 공격력
    double attackInterval;       // 공격 간격(초)

    int productionAmount;        // 생산 1회당 자원량
    double productionInterval;   // 생산 주기(초)

    double placementCooldown;    // 배치 재사용 대기시간(초)
    int cost;                    // 설치 비용
} UnitSpec;

/* 실제 배치된 유닛의 개별 상태 */
typedef struct {
    bool active;
    UnitType type;

    int row;
    int col;
    int currentHp;               // 해당 유닛의 현재 체력

    double attackElapsed;        // 공격 시간 누적
    double productionElapsed;    // 생산 시간 누적
} Unit;

/* 유닛 종류 및 기본 능력치 설정 */
void initUnitTypes(UnitSpec specs[UNIT_TYPE_COUNT])
{
    specs[UNIT_ATTACK] = (UnitSpec){
        .name = "공격형",
        .maxHp = 100,
        .attackPower = 10,
        .attackInterval = 0.5,
        .productionAmount = 0,
        .productionInterval = 0.0,
        .placementCooldown = 15.0,
        .cost = 200
    };

    specs[UNIT_DEFENSE] = (UnitSpec){
        .name = "방어형",
        .maxHp = 300,
        .attackPower = 0,
        .attackInterval = 0.0,
        .productionAmount = 0,
        .productionInterval = 0.0,
        .placementCooldown = 20.0,
        .cost = 300
    };

    specs[UNIT_PRODUCER] = (UnitSpec){
        .name = "자원 생산형",
        .maxHp = 50,
        .attackPower = 0,
        .attackInterval = 0.0,
        .productionAmount = 25,
        .productionInterval = 4.0,
        .placementCooldown = 10.0,
        .cost = 100
    };
}

/*
 * 기본 정보를 참조하여 개별 유닛의 초기 상태 생성
 * 배치 가능 여부 검사와 비용 차감은 placeUnit()에서 담당한다.
 */
bool initUnit(
    Unit* unit,
    const UnitSpec specs[UNIT_TYPE_COUNT],
    UnitType type,
    int row,
    int col
)
{
    if (unit == NULL || specs == NULL) {
        return false;
    }

    if ((unsigned int)type >= (unsigned int)UNIT_TYPE_COUNT) {
        return false;
    }

    *unit = (Unit){
        .active = true,
        .type = type,
        .row = row,
        .col = col,
        .currentHp = specs[type].maxHp,
        .attackElapsed = 0.0,
        .productionElapsed = 0.0
    };

    return true;
}

/* 종류별 능력치 출력 */
void printUnitTypes(const UnitSpec specs[UNIT_TYPE_COUNT])
{
    for (int i = 0; i < UNIT_TYPE_COUNT; i++) {
        const UnitSpec* spec = &specs[i];

        printf("[%s 유닛]\n", spec->name);
        printf("기본 체력: %d\n", spec->maxHp);
        printf("공격력: %d\n", spec->attackPower);

        if (spec->attackPower > 0 &&
            spec->attackInterval > 0.0) {
            printf("공격 간격: %.1f초\n",
                spec->attackInterval);
        }
        else {
            printf("공격: 없음\n");
        }

        if (spec->productionAmount > 0 &&
            spec->productionInterval > 0.0) {
            printf("자원 생산: %.1f초마다 %d\n",
                spec->productionInterval,
                spec->productionAmount);
        }
        else {
            printf("자원 생산: 없음\n");
        }

        printf("배치 재사용 대기시간: %.1f초\n",
            spec->placementCooldown);
        printf("설치 비용: %d\n\n", spec->cost);
    }
}

/* 초기화 기능 실행 */
void runUnitTypeSystem(void)
{
    UnitSpec specs[UNIT_TYPE_COUNT];

    initUnitTypes(specs);
    printUnitTypes(specs);
}


/* 지형 정보 */
typedef struct {
    bool buildable;  // 유닛 배치 가능 여부
} Terrain;

/* 유닛 점유 정보 */
typedef struct {
    int unitId;      // -1: 빈칸, 0 이상: 배치된 유닛 번호
} CellState;

/* 전체 필드 정보 */
typedef struct {
    Terrain terrain[ROWS][COLS];
    CellState cells[ROWS][COLS];

    int baseBoundary;   // 왼쪽 기지 경계
    int spawnBoundary;  // 오른쪽 적 등장 경계
} Field;

/* 필드 및 배치 구역 초기화 */
void initField(Field* field)
{
    if (field == NULL) {
        return;
    }

    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            /* 기본 맵은 모든 칸에 유닛 배치 가능 */
            field->terrain[row][col].buildable = true;

            /* 기존 유닛 점유 정보 제거 */
            field->cells[row][col].unitId = NO_UNIT;
        }
    }

    /*
     * 필드의 실제 열 인덱스: 0~8
     * 양쪽 경계는 필드 바깥의 위치로 정의
     */
    field->baseBoundary = -1;
    field->spawnBoundary = COLS;
}

/* 필드 출력 */
void printField(const Field* field)
{
    if (field == NULL) {
        return;
    }

    printf("       ");

    for (int col = 0; col < COLS; col++) {
        printf("%d ", col + 1);
    }
    printf("\n");

    for (int row = 0; row < ROWS; row++) {
        printf("%d  B | ", row + 1);

        for (int col = 0; col < COLS; col++) {
            char symbol;

            if (field->cells[row][col].unitId != NO_UNIT) {
                symbol = 'U';
            }
            else if (!field->terrain[row][col].buildable) {
                symbol = '#';
            }
            else {
                symbol = '.';
            }

            printf("%c ", symbol);
        }

        printf("| E\n");
    }

    printf("\n.: 배치 가능한 빈칸\n");
    printf("#: 배치 불가 / U: 배치된 유닛\n");
    printf("B: 기지 경계 / E: 적 등장 경계\n");
}

/* 초기화 기능 실행 */
void runFieldExample(void)
{
    Field field;

    initField(&field);
    printField(&field);
}
/* 수집 가능한 자원 */
typedef struct {
    bool active;
    int amount;
} Resource;

/* 생산 유닛 상태 */
typedef struct {
    bool alive;
    double elapsed;
    double interval;
    int amount;
} Producer;

/* 자원 관리 상태 */
typedef struct {
    long long ownedResources;
    bool paused;

    double baseElapsed;

    Resource resources[MAX_RESOURCES];
    Producer producer;
} ResourceState;

/* 초기화 */
void initResources(ResourceState* state)
{
    *state = (ResourceState){ 0 };

    /* 현재는 생산 유닛 한 개가 있는 상태로 시작 */
    state->producer.alive = true;
    state->producer.interval = PRODUCER_INTERVAL;
    state->producer.amount = PRODUCER_AMOUNT;
}

/* 빈 공간에 자원 생성 */
bool spawnResource(ResourceState* state, int amount)
{
    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (!state->resources[i].active) {
            state->resources[i].active = true;
            state->resources[i].amount = amount;

            return true;
        }
    }

    return false;
}

/* 활성화된 자원을 모두 수집 */
void collectResources(ResourceState* state)
{
    for (int i = 0; i < MAX_RESOURCES; i++) {
        Resource* resource = &state->resources[i];

        if (resource->active) {
            state->ownedResources += resource->amount;

            /* 이미 수집한 자원이 다시 획득되지 않도록 제거 */
            resource->active = false;
            resource->amount = 0;
        }
    }
}

/* 자원 생성 및 수집 갱신 */
void updateResources(
    ResourceState* state,
    double deltaTime,
    bool collectRequested
)
{
    if (state == NULL || state->paused || deltaTime < 0.0) {
        return;
    }

    /* 수집 요청 처리 */
    if (collectRequested) {
        collectResources(state);
    }

    /* 기본 자원 생성 */
    state->baseElapsed += deltaTime;

    while (state->baseElapsed >= BASE_INTERVAL) {
        if (!spawnResource(state, BASE_AMOUNT)) {
            /*
             * 저장 공간이 없으면 생성을 보류한다.
             * 누적 시간은 유지한다.
             */
            break;
        }

        state->baseElapsed -= BASE_INTERVAL;
    }

    /* 살아 있는 생산 유닛만 생산 */
    Producer* producer = &state->producer;

    if (producer->alive && producer->interval > 0.0) {
        producer->elapsed += deltaTime;

        while (producer->elapsed >= producer->interval) {
            if (!spawnResource(state, producer->amount)) {
                break;
            }

            producer->elapsed -= producer->interval;
        }
    }
}

/* 수집 대기 중인 자원량 계산 */
int getAvailableResources(const ResourceState* state)
{
    int total = 0;

    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (state->resources[i].active) {
            total += state->resources[i].amount;
        }
    }

    return total;
}

/* 다음 생성까지 남은 시간 계산 */
double getRemainingTime(double interval, double elapsed)
{
    double remaining = interval - elapsed;

    return remaining > 0.0 ? remaining : 0.0;
}

/* 같은 줄에 자원 상태를 실시간으로 출력 */
void printResources(const ResourceState* state)
{
    double baseRemaining =
        getRemainingTime(BASE_INTERVAL, state->baseElapsed);

    double producerRemaining = 0.0;

    if (state->producer.alive) {
        producerRemaining = getRemainingTime(
            state->producer.interval,
            state->producer.elapsed
        );
    }

    printf(
        "\r[%s] 보유: %lld | 수집 가능: %d"
        " | 기본: %.1f초 | 생산 유닛: %.1f초      ",
        state->paused ? "정지" : "진행",
        state->ownedResources,
        getAvailableResources(state),
        baseRemaining,
        producerRemaining
    );

    /* 개행 없이도 즉시 화면에 반영 */
    fflush(stdout);
}


#define MAX_UNITS (ROWS * COLS)

/* 배치에 필요한 전체 상태 */
typedef struct {
    Field field;
    UnitSpec specs[UNIT_TYPE_COUNT];
    Unit units[MAX_UNITS];

    

    /* 종류별 다음 배치 가능 시각: 밀리초 */
    ULONGLONG nextPlacementTime[UNIT_TYPE_COUNT];
} PlacementState;

/* 배치 결과 */
typedef enum {
    PLACE_SUCCESS,
    PLACE_INVALID_STATE,
    PLACE_INVALID_TYPE,
    PLACE_OUT_OF_RANGE,
    PLACE_BLOCKED,
    PLACE_OCCUPIED,
    PLACE_NOT_ENOUGH_RESOURCES,
    PLACE_COOLDOWN_ACTIVE
} PlaceResult;

/* 배치 시스템 초기화 */
void initPlacementSystem(PlacementState* state)
{
    *state = (PlacementState){ 0 };

    initField(&state->field);
    initUnitTypes(state->specs);

    /*
     * 배치 기능을 확인하기 위한 시작 자원이다.
     * 실제 게임의 시작 자원으로 변경할 수 있다.
     */
   
}

/* 유닛 배치 */
PlaceResult placeUnit(
    PlacementState* state,
    ResourceState* resources,
    UnitType type,
    int row,
    int col
)
{
    if (state == NULL || resources == NULL) {
        return PLACE_INVALID_STATE;
    }

    if ((unsigned int)type >= (unsigned int)UNIT_TYPE_COUNT) {
        return PLACE_INVALID_TYPE;
    }

    /* 배열 접근 전에 좌표를 검사한다. */
    if (row < 0 || row >= ROWS ||
        col < 0 || col >= COLS) {
        return PLACE_OUT_OF_RANGE;
    }

    if (!state->field.terrain[row][col].buildable) {
        return PLACE_BLOCKED;
    }

    if (state->field.cells[row][col].unitId != NO_UNIT) {
        return PLACE_OCCUPIED;
    }

    const UnitSpec* spec = &state->specs[type];

    if (resources->ownedResources < spec->cost) {
        return PLACE_NOT_ENOUGH_RESOURCES;
    }

    ULONGLONG now = GetTickCount64();

    if (now < state->nextPlacementTime[type]) {
        return PLACE_COOLDOWN_ACTIVE;
    }

    /*
     * 한 칸에 한 유닛만 배치하므로
     * 칸마다 고정된 유닛 저장 위치를 사용한다.
     */
    int unitId = row * COLS + col;

    /*
     * 먼저 임시 유닛을 생성한다.
     * 이 단계까지 게임 상태는 변경하지 않는다.
     */
    Unit newUnit;

    if (!initUnit(&newUnit, state->specs, type, row, col)) {
        return PLACE_INVALID_STATE;
    }

    /* 모든 검사가 끝난 뒤 성공 결과를 반영한다. */
    state->units[unitId] = newUnit;
    state->field.cells[row][col].unitId = unitId;

    resources->ownedResources -= spec->cost;

    state->nextPlacementTime[type] =
        now + (ULONGLONG)(spec->placementCooldown * 1000.0);

    return PLACE_SUCCESS;
}

/* 결과에 따른 안내 */
void printPlacementResult(PlaceResult result)
{
    switch (result) {
    case PLACE_SUCCESS:
        printf("유닛을 배치했습니다.\n");
        break;

    case PLACE_INVALID_STATE:
        printf("게임 상태를 확인할 수 없습니다.\n");
        break;

    case PLACE_INVALID_TYPE:
        printf("잘못된 유닛 종류입니다.\n");
        break;

    case PLACE_OUT_OF_RANGE:
        printf("행은 1~5, 열은 1~9 범위여야 합니다.\n");
        break;

    case PLACE_BLOCKED:
        printf("유닛을 배치할 수 없는 구역입니다.\n");
        break;

    case PLACE_OCCUPIED:
        printf("이미 유닛이 배치된 칸입니다.\n");
        break;

    case PLACE_NOT_ENOUGH_RESOURCES:
        printf("보유 자원이 부족합니다.\n");
        break;

    case PLACE_COOLDOWN_ACTIVE:
        printf("해당 종류의 재사용 대기시간이 남아 있습니다.\n");
        break;
    }
}

/* 현재 자원과 종류별 대기시간 출력 */
void printPlacementStatus(
    const PlacementState* state,
    const ResourceState* resources
)
{
    ULONGLONG now = GetTickCount64();

    printf("\n보유 자원: %lld\n", resources->ownedResources);

    for (int i = 0; i < UNIT_TYPE_COUNT; i++) {
        double remaining = 0.0;

        if (state->nextPlacementTime[i] > now) {
            remaining =
                (double)(state->nextPlacementTime[i] - now)
                / 1000.0;
        }

        printf("%d. %s | 비용: %d | 남은 대기시간: %.1f초\n",
            i + 1,
            state->specs[i].name,
            state->specs[i].cost,
            remaining);
    }
}

/* 적 한 개의 전투 상태 */
typedef struct {
    bool active;
    int row;
    double x;                /* 현재 열 위치 */
    int hp;
    int attackPower;
    double attackInterval;   /* 공격 간격(초) */
    double attackElapsed;    /* 접촉 후 누적된 공격 시간 */
} CombatEnemy;

/* 투사체 한 개의 상태 */
typedef struct {
    bool active;
    int row;
    double x;
    int damage;
} Projectile;

/* 적과 투사체의 공통 저장소 */
typedef struct {
    CombatEnemy enemies[MAX_ENEMIES];
    Projectile shots[MAX_PROJECTILES];
    double accumulatedTime;
} CombatState;

/* 같은 행 앞쪽에 살아 있는 적이 있는지 검사 */
static bool hasEnemyAhead(const CombatState* combat, const Unit* unit)
{
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const CombatEnemy* enemy = &combat->enemies[i];

        if (enemy->active && enemy->hp > 0 &&
            enemy->row == unit->row && enemy->x > unit->col) {
            return true;
        }
    }
    return false;
}

/* 빈 투사체 슬롯에 한 발 생성 */
static bool fireProjectile(CombatState* combat,
    const Unit* unit, int damage)
{
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!combat->shots[i].active) {
            combat->shots[i] = (Projectile){
                .active = true,
                .row = unit->row,
                .x = (double)unit->col,
                .damage = damage
            };
            return true;
        }
    }
    return false;
}

/* 공격형 유닛의 자동 발사 */
static void updateUnitFire(PlacementState* placement,
    CombatState* combat, double dt)
{
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit* unit = &placement->units[i];

        if (!unit->active || unit->type != UNIT_ATTACK) {
            continue;
        }

        const UnitSpec* spec = &placement->specs[unit->type];

        if (spec->attackPower <= 0 || spec->attackInterval <= 0.0) {
            continue;
        }

        unit->attackElapsed += dt;

        /* 적이 없는 동안 공격 횟수가 무한히 쌓이지 않도록 한다. */
        if (unit->attackElapsed >= spec->attackInterval) {
            unit->attackElapsed = spec->attackInterval;

            if (hasEnemyAhead(combat, unit) &&
                fireProjectile(combat, unit, spec->attackPower)) {
                unit->attackElapsed = 0.0;
            }
        }
    }
}

/* 투사체 이동, 첫 충돌 판정, 적 피해 처리 */
static void updateProjectiles(CombatState* combat, double dt)
{
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile* shot = &combat->shots[i];

        if (!shot->active) {
            continue;
        }

        double oldX = shot->x;
        double newX = oldX + SHOT_SPEED * dt;
        int hitIndex = -1;

        for (int j = 0; j < MAX_ENEMIES; j++) {
            const CombatEnemy* enemy = &combat->enemies[j];

            if (!enemy->active || enemy->hp <= 0 ||
                enemy->row != shot->row) {
                continue;
            }

            /* 이번 이동 구간 안에 있고 필드 안에 있는 적만 검사 */
            if (enemy->x < oldX || enemy->x > newX ||
                enemy->x >= COLS) {
                continue;
            }

            /* 배열에서 먼저 나온 적이 아니라 실제로 가까운 적 선택 */
            if (hitIndex < 0 ||
                enemy->x < combat->enemies[hitIndex].x) {
                hitIndex = j;
            }
        }

        if (hitIndex >= 0) {
            CombatEnemy* enemy = &combat->enemies[hitIndex];

            enemy->hp -= shot->damage;
            if (enemy->hp <= 0) {
                enemy->hp = 0;
                enemy->active = false;
            }

            shot->active = false;
        }
        else if (newX >= COLS) {
            shot->active = false;
        }
        else {
            shot->x = newX;
        }
    }
}

/* 적의 바로 왼쪽에 맞닿아 있는 유닛을 찾는다. */
static int findContactUnit(const PlacementState* placement,
    const CombatEnemy* enemy)
{
    if (enemy->row < 0 || enemy->row >= ROWS) {
        return NO_UNIT;
    }

    for (int col = COLS - 1; col >= 0; col--) {
        double distance = enemy->x - col;

        /* 같은 행에서 오른쪽으로 반 칸 이내이면 접촉 상태 */
        if (distance < 0.0 || distance > 0.500001) {
            continue;
        }

        int id = placement->field.cells[enemy->row][col].unitId;

        if (id >= 0 && id < MAX_UNITS &&
            placement->units[id].active) {
            return id;
        }
    }
    return NO_UNIT;
}

/* 유닛 피해 처리: 파괴되면 해당 칸을 다시 비운다. */
static void damagePlacedUnit(PlacementState* placement,
    int unitId, int damage)
{
    Unit* unit = &placement->units[unitId];

    if (!unit->active || damage <= 0) {
        return;
    }

    unit->currentHp -= damage;

    if (unit->currentHp <= 0) {
        unit->currentHp = 0;
        unit->active = false;
        placement->field.cells[unit->row][unit->col].unitId = NO_UNIT;
    }
}

/* 유닛과 접촉한 적의 근접 공격 */
static void updateEnemyAttacks(PlacementState* placement,
    CombatState* combat, double dt)
{
    for (int i = 0; i < MAX_ENEMIES; i++) {
        CombatEnemy* enemy = &combat->enemies[i];

        if (!enemy->active || enemy->hp <= 0 ||
            enemy->attackPower <= 0 || enemy->attackInterval <= 0.0) {
            continue;
        }

        /* 매번 다시 검사하므로 이미 파괴된 유닛은 공격하지 않는다. */
        int unitId = findContactUnit(placement, enemy);

        if (unitId == NO_UNIT) {
            enemy->attackElapsed = 0.0;
            continue;
        }

        enemy->attackElapsed += dt;

        if (enemy->attackElapsed >= enemy->attackInterval) {
            enemy->attackElapsed -= enemy->attackInterval;
            damagePlacedUnit(placement, unitId, enemy->attackPower);
        }
    }
}

/* runGame()이 호출하는 대표 전투 함수 */
void updateCombat(PlacementState* placement,
    CombatState* combat, double deltaTime)
{
    if (placement == NULL || combat == NULL || deltaTime <= 0.0) {
        return;
    }

    combat->accumulatedTime += deltaTime;

    while (combat->accumulatedTime >= COMBAT_STEP) {
        updateUnitFire(placement, combat, COMBAT_STEP);
        updateProjectiles(combat, COMBAT_STEP);
        updateEnemyAttacks(placement, combat, COMBAT_STEP);

        combat->accumulatedTime -= COMBAT_STEP;
    }
}

/* 실시간으로 유닛이 배치가 되었는지 보는 함수 */

/* 해당 칸에 실제로 배치된 유닛을 문자로 반환 */
static char getCellSymbol(const PlacementState* state, int row, int col)
{
    int unitId = state->field.cells[row][col].unitId;

    if (unitId == NO_UNIT) {
        return state->field.terrain[row][col].buildable ? '.' : '#';
    }

    /* 잘못된 번호로 유닛 배열에 접근하지 않도록 검사 */
    if (unitId < 0 || unitId >= MAX_UNITS) {
        return '?';
    }

    const Unit* unit = &state->units[unitId];

    if (!unit->active) {
        return '?';  /* 칸은 점유 중인데 유닛은 비활성인 경우 */
    }

    switch (unit->type) {
    case UNIT_ATTACK:   return 'A';
    case UNIT_DEFENSE:  return 'D';
    case UNIT_PRODUCER: return 'P';
    default:            return '?';
    }
}

/* 필드, 선택 위치, 자원, 배치 결과를 한 화면에 출력 */
static void drawGameScreen(
    const PlacementState* state,
    const ResourceState* resources,
    UnitType selectedType,
    int selectedRow,
    int selectedCol,
    const char* message
)
{
    /* 화면을 매번 지우지 않고 같은 위치에 덮어쓴다. */
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD origin = { 0, 0 };
    SetConsoleCursorPosition(console, origin);

    printf("=== 유닛 배치형 디펜스 ===\n");
    printf("1: 공격형 | 2: 방어형 | 3: 생산형 | WASD: 이동\n");
    printf("Enter: 배치 | Space: 자원 수집 | Q: 종료\n\n");

    /* 열 번호 */
    printf("     ");
    for (int col = 0; col < COLS; col++) {
        printf(" %d ", col + 1);
    }
    printf("\n");

    /* 행 번호와 각 칸의 상태 */
    for (int row = 0; row < ROWS; row++) {
        printf("%2d | ", row + 1);

        for (int col = 0; col < COLS; col++) {
            char symbol = getCellSymbol(state, row, col);

            if (row == selectedRow && col == selectedCol) {
                printf("[%c]", symbol);
            }
            else {
                printf(" %c ", symbol);
            }
        }
        printf("\n");
    }

    printf("\nA: 공격형 | D: 방어형 | P: 생산형 | .: 빈칸 | #: 배치 불가\n");

    /* 숫자와 메시지 뒤의 공백은 이전 출력의 잔여 문자를 지운다. */
    printf("보유 자원: %-8lld | 수집 가능: %-8d\n",
        resources->ownedResources,
        getAvailableResources(resources));

    printf("선택: %s | 위치: %d행 %d열 | 설치 비용: %d          \n",
        state->specs[selectedType].name,
        selectedRow + 1,
        selectedCol + 1,
        state->specs[selectedType].cost);

    /* 종류별 배치 대기시간 표시 */
    ULONGLONG now = GetTickCount64();

    printf("배치 대기 | ");

    for (int i = 0; i < UNIT_TYPE_COUNT; i++) {
        ULONGLONG readyTime = state->nextPlacementTime[i];

        printf("%s: ", state->specs[i].name);

        if (readyTime > now) {
            ULONGLONG remainingMs = readyTime - now;

            /* 0.1초가 남아도 0초가 아닌 1초로 표시 */
            unsigned int remainingSec =
                (unsigned int)((remainingMs + 999) / 1000);

            printf("%2u초", remainingSec);
        }
        else {
            printf("준비");
        }

        if (i < UNIT_TYPE_COUNT - 1) {
            printf(" | ");
        }
    }

    printf("      \n");   // 이전 출력의 잔여 문자 지우기
    printf("상태: %s                                \n", message);
    fflush(stdout);
}



/* 자원 생성과 배치를 하나의 반복문에서 처리 */
void runGame(void)
{
    ResourceState resources;
    PlacementState placement;
    CombatState combat = { 0 };

    initResources(&resources);
    initPlacementSystem(&placement);

    resources.ownedResources = 0;

    UnitType selectedType = UNIT_ATTACK;
    int selectedRow = 0;
    int selectedCol = 0;

    bool running = true;
    ULONGLONG previousTime = GetTickCount64();

    const char* message = "유닛과 위치를 선택하세요.";

    system("cls");   // Windows 콘솔에서 게임 시작 시 한 번만 지운다.

    while (running) {
        /* 실제 경과 시간 계산 */
        ULONGLONG currentTime = GetTickCount64();

        double deltaTime =
            (double)(currentTime - previousTime) / 1000.0;

        previousTime = currentTime;

        /* 입력하지 않아도 자원 생성은 계속 진행 */
        updateResources(&resources, deltaTime, false);

        /* 입력이 있을 때만 키를 읽는다. */
        while (_kbhit()) {
            int key = _getch();

            if (key == 0 || key == 224) {
                (void)_getch();
                continue;
            }

            switch (key) {
            case '1':
                selectedType = UNIT_ATTACK;
                break;

            case '2':
                selectedType = UNIT_DEFENSE;
                break;

            case '3':
                selectedType = UNIT_PRODUCER;
                break;

            case 'w':
            case 'W':
                if (selectedRow > 0) {
                    selectedRow--;
                }
                break;

            case 's':
            case 'S':
                if (selectedRow < ROWS - 1) {
                    selectedRow++;
                }
                break;

            case 'a':
            case 'A':
                if (selectedCol > 0) {
                    selectedCol--;
                }
                break;

            case 'd':
            case 'D':
                if (selectedCol < COLS - 1) {
                    selectedCol++;
                }
                break;

            case ' ':
                updateResources(&resources, 0.0, true);
                message = "자원을 수집했습니다.";
                break;

            case '\r': {
                /* 공통 자원 상태를 함께 전달 */
                PlaceResult result = placeUnit(
                    &placement,
                    &resources,
                    selectedType,
                    selectedRow,
                    selectedCol
                );

                switch (result) {
                case PLACE_SUCCESS:
                    message = "배치 성공";
                    break;

                case PLACE_OCCUPIED:
                    message = "이미 유닛이 있는 칸";
                    break;

                case PLACE_NOT_ENOUGH_RESOURCES:
                    message = "자원 부족";
                    break;

                case PLACE_COOLDOWN_ACTIVE:
                    message = "배치 대기시간 미완료";
                    break;

                case PLACE_BLOCKED:
                    message = "배치 불가능한 구역";
                    break;

                default:
                    message = "배치 실패";
                    break;
                }
                break;
            }

            case 'q':
            case 'Q':
                running = false;
                break;
            }

            if (!running) {
                break;
            }
        }

        if (!running) {
            break;
        }

        updateCombat(&placement, &combat, deltaTime);

        drawGameScreen(
            &placement,
            &resources,
            selectedType,
            selectedRow,
            selectedCol,
            message
        );

        Sleep(50);
    }

    printf("\n게임을 종료합니다.\n");
}


/* main에서는 실행 함수 호출 */
int main(void)
{
    runUnitTypeSystem();
    runFieldExample();
    runGame();
    return 0;
}
