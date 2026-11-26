#define _CRT_SECURE_NO_WARMNINGS
#include <stdio.h>
#include <stdbool.h>
#include <windows.h>
#include <conio.h>

#define MAX_RESOURCES 100

#define BASE_INTERVAL 5.0
#define BASE_AMOUNT 25

#define PRODUCER_INTERVAL 4.0
#define PRODUCER_AMOUNT 25
#define ROWS 5
#define COLS 9
#define NO_UNIT (-1)

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

/* 자원 생성과 배치를 하나의 반복문에서 처리 */
void runGame(void)
{
    ResourceState resources;
    PlacementState placement;

    initResources(&resources);
    initPlacementSystem(&placement);

    resources.ownedResources = 0;

    UnitType selectedType = UNIT_ATTACK;
    int selectedRow = 0;
    int selectedCol = 0;

    bool running = true;
    ULONGLONG previousTime = GetTickCount64();

    const char* message = "유닛과 위치를 선택하세요.";

    printf("1: 공격형 / 2: 방어형 / 3: 생산형\n");
    printf("WASD: 위치 이동 / Enter: 배치\n");
    printf("SPACE: 자원 수집 / Q: 종료\n\n");

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

        printf(
            "\r보유:%lld 수집:%d | %s (%d,%d) | %s"
            "                              ",
            resources.ownedResources,
            getAvailableResources(&resources),
            placement.specs[selectedType].name,
            selectedRow + 1,
            selectedCol + 1,
            message
        );
        fflush(stdout);

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
