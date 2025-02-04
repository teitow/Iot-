#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>

// 디바이스 경로 정의
#define fnd "/dev/fnd"
#define dot "/dev/dot"
#define tact "/dev/tactsw"
#define led "/dev/led"
#define dip "/dev/dipsw"
#define clcd "/dev/clcd"

// 함수 선언부
void print_clcd(const char* message);
void writeToDotDevice(unsigned char* data, int time);
int tactsw_get_with_timer(int t_second);
int dipsw_get_with_timer(int t_second);
void led_on(int strikes, int balls, int outs, int homerun);
void init_devices();
void game_rule(int round);
void start_game();
void input_number(char* number, int digits);
void check_guess(const char* guess, const char* secret, int length, int* strikes, int* balls);
void display_score(int player1_score, int player2_score);
void print_game_start();
void print_round_start(int round);
void print_input_number();
void print_result(int strikes, int balls, int outs, int homerun);
void print_final_score(int player1_score, int player2_score);
void print_game_over();
int intro();
int is_valid_number(const char* number, int length);

// 전역 변수
int dipsw;
int leds;
int dot_mtx;
int tactsw;
int clcds;
int fnds;
unsigned char fnd_data[4];

// Dot Matrix 패턴
unsigned char patterns[4][8] = {
    {0x3C, 0x42, 0x40, 0x3C, 0x02, 0x02, 0x42, 0x3C}, // STRIKE
    {0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C}, // OUT
    {0x42, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x42}, // HOME RUN
    {0x1E, 0x22, 0x22, 0x1E, 0x22, 0x22, 0x22, 0x1E}  // BALL
};

// Character LCD 함수
void print_clcd(const char* message) {
    clcds = open(clcd, O_RDWR);
    if (clcds < 0) {
        printf("Character LCD 열기 실패.\n");
        exit(0);
    }
    write(clcds, message, strlen(message));
    close(clcds);
}

void print_game_start() {
    print_clcd("Game Start!");
    usleep(2000000);  // 2초 대기
}

void print_round_start(int round) {
    char buffer[32];
    sprintf(buffer, "Round %d Start!", round);
    print_clcd(buffer);
    usleep(2000000);  // 2초 대기
}

void print_result(int strikes, int balls, int outs, int homerun) {
    char buffer[32];
    sprintf(buffer, "S:%d B:%d O:%d H:%d", strikes, balls, outs, homerun);
    print_clcd(buffer);
    usleep(2000000);  // 2초 대기
}

void print_final_score(int player1_score, int player2_score) {
    char buffer[32];
    sprintf(buffer, "P1: %d P2: %d", player1_score, player2_score);
    print_clcd(buffer);
    usleep(2000000);  // 2초 대기
}

void print_game_over() {
    print_clcd("Game Over");
    usleep(2000000);  // 2초 대기
}

// Dot Matrix 함수
void writeToDotDevice(unsigned char* data, int time) {
    dot_mtx = open(dot, O_RDWR);
    if (dot_mtx < 0) {
        printf("Dot 디바이스 열기 실패\n");
        exit(0);
    }
    write(dot_mtx, data, 8);
    usleep(time);
    close(dot_mtx);
}

// TACT Switch 함수
int tactsw_get_with_timer(int t_second) {
    int tactswFd, selected_tact = 0;
    unsigned char b = 0;

    tactswFd = open(tact, O_RDONLY);
    if (tactswFd < 0) {
        perror("TACT 스위치 디바이스 열기 실패");
        return -1;
    }

    while (t_second > 0) {
        usleep(100000); // 100ms 대기
        read(tactswFd, &b, sizeof(b));
        if (b) {
            selected_tact = b;
            close(tactswFd);
            return selected_tact;
        }
        t_second -= 0.1;
    }

    close(tactswFd);
    return 0; // 타임아웃
}

// DIP Switch 함수
int dipsw_get_with_timer(int t_second) {
    int dipswFd, selected_dip = 0;
    unsigned char d = 0;

    dipswFd = open(dip, O_RDONLY);
    if (dipswFd < 0) {
        perror("DIP 스위치 디바이스 열기 실패");
        return -1;
    }

    while (t_second > 0) {
        usleep(100000); // 100ms 대기
        read(dipswFd, &d, sizeof(d));
        if (d) {
            selected_dip = d;
            close(dipswFd);
            return selected_dip;
        }
        t_second -= 0.1;
    }

    close(dipswFd);
    return 0; // 타임아웃
}

// LED 제어 함수
void led_on(int strikes, int balls, int outs, int homerun) {
    unsigned char led_data = 0;
    if (strikes > 0) led_data |= 0x22; // Green LEDs
    if (balls > 0) led_data |= 0x44;   // Yellow LEDs
    if (outs > 0) led_data |= 0x11;    // Red LEDs
    if (homerun > 0) led_data |= 0x88; // Blue LEDs
    leds = open(led, O_RDWR);
    if (leds < 0) {
        printf("LED 열기 실패.\n");
        exit(0);
    }
    write(leds, &led_data, sizeof(unsigned char));
    close(leds);
}

// 디바이스 초기화 함수
void init_devices() {
    dipsw = open(dip, O_RDWR);
    leds = open(led, O_RDWR);
    dot_mtx = open(dot, O_RDWR);
    tactsw = open(tact, O_RDWR);
    clcds = open(clcd, O_RDWR);
    fnds = open(fnd, O_RDWR);
    if (dipsw < 0 || leds < 0 || dot_mtx < 0 || tactsw < 0 || clcds < 0 || fnds < 0) {
        printf("디바이스 열기 실패\n");
        exit(0);
    }
    close(dipsw);
    close(leds);
    close(dot_mtx);
    close(tactsw);
    close(clcds);
    close(fnds);
}

// 게임 규칙 출력 함수
void game_rule(int round) {
    if (round == 1) {
        print_clcd("Round 1, 3 digits");
    }
    else if (round == 2) {
        print_clcd("Round 2, 4 digits");
    }
    usleep(3000000);
}

// 숫자 유효성 검사 함수
int is_valid_number(const char* number, int length) {
    if (strlen(number) != length) return 0; // 지정된 길이가 아님
    for (int i = 0; i < length; i++) {
        for (int j = i + 1; j < length; j++) {
            if (number[i] == number[j]) {
                return 0; // 중복된 숫자 발견
            }
        }
    }
    return 1; // 유효한 입력
}

// 사용자 추측 검사 및 스트라이크, 볼 계산
void check_guess(const char* guess, const char* secret, int length, int* strikes, int* balls) {
    *strikes = 0;
    *balls = 0;
    for (int i = 0; i < length; i++) {
        for (int j = 0; j < length; j++) {
            if (guess[i] == secret[j]) {
                if (i == j)
                    (*strikes)++;
                else
                    (*balls)++;
            }
        }
    }
}

// FND 출력 함수
void PrintFnd(const char* number) {
    unsigned char fnd_codes[10] = { 0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xD8, 0x80, 0x98 };

    // FND 데이터를 초기화합니다.
    for (int i = 0; i < 4; i++) {
        if (i < strlen(number)) {
            fnd_data[i] = fnd_codes[number[i] - '0'];
        }
        else {
            fnd_data[i] = 0xFF;
        }
    }

    int fndFd = open(fnd, O_RDWR);
    if (fndFd < 0) {
        perror("FND 디바이스 열기 실패");
        return;
    }
    write(fndFd, fnd_data, sizeof(fnd_data));
    close(fndFd);
}

void ResetFnd() {
    // FND 데이터를 초기화합니다.
    fnd_data[0] = 0xFF;
    fnd_data[1] = 0xFF;
    fnd_data[2] = 0xFF;
    fnd_data[3] = 0xFF;

    int fndFd = open(fnd, O_RDWR);
    if (fndFd < 0) {
        perror("FND 디바이스 열기 실패");
        return;
    }
    write(fndFd, fnd_data, sizeof(fnd_data));
    close(fndFd);
}

// 점수 표시 함수
void display_score(int player1_score, int player2_score) {
    char score[32];
    sprintf(score, "P1: %d, P2: %d", player1_score, player2_score);
    print_clcd(score);
    usleep(5000000); // 5초 대기
}

// 숫자 입력 함수
void input_number(char* number, int digits) {
    int tactsw_value;
    int idx = 0;

    while (idx < digits) {
        tactsw_value = tactsw_get_with_timer(100);
        if (tactsw_value >= 1 && tactsw_value <= 9) {
            number[idx++] = '0' + tactsw_value;
            PrintFnd(number); // 입력 중인 숫자를 FND에 표시
        }
    }
    number[digits] = '\0';
}

// 게임 시작 함수
void start_game() {
    char secret_number1[5]; // 플레이어 1의 비밀 숫자 저장 (최대 4자리)
    char secret_number2[5]; // 플레이어 2의 비밀 숫자 저장
    char guess[5];          // 추측 저장 (최대 4자리)
    int strikes, balls;
    int turn = 1;
    int score1 = 1000, score2 = 1000; // 게임 시작 점수
    int rounds = 2;                   // 총 라운드 수
    int digits[2] = { 3, 4 };         // 각 라운드의 자릿수

    for (int round = 0; round < rounds; round++) {
        int current_digits = digits[round];
        game_rule(round + 1);

        // 플레이어 1과 2의 비밀 숫자 설정
        for (int player = 1; player <= 2; player++) {
            while (1) {
                print_clcd(player == 1 ? "P1 Set Number:" : "P2 Set Number:");
                input_number(player == 1 ? secret_number1 : secret_number2, current_digits);
                if (!is_valid_number(player == 1 ? secret_number1 : secret_number2, current_digits)) {
                    print_clcd("Invalid Number!");
                    usleep(2000000); // 2초 대기
                }
                else {
                    break; // 유효한 입력
                }
            }
        }

        // 게임 진행
        int home_run1 = 0, home_run2 = 0;
        while (home_run1 == 0 || home_run2 == 0) {
            if ((turn == 1 && home_run1) || (turn == 2 && home_run2)) {
                turn = turn == 1 ? 2 : 1; // 홈런을 친 플레이어는 건너뛰기
                continue;
            }

            while (1) {
                print_clcd(turn == 1 ? "P1 Guess:" : "P2 Guess:");
                input_number(guess, current_digits);
                if (!is_valid_number(guess, current_digits)) {
                    print_clcd("Invalid Guess!");
                    usleep(2000000); // 2초 대기
                }
                else {
                    break; // 유효한 입력
                }
            }

            if (turn == 1) {
                check_guess(guess, secret_number2, current_digits, &strikes, &balls);
                if (strikes == current_digits) home_run1 = 1;
                score1 -= (10 * (current_digits - strikes) + balls); // 점수 감점
            }
            else {
                check_guess(guess, secret_number1, current_digits, &strikes, &balls);
                if (strikes == current_digits) home_run2 = 1;
                score2 -= (10 * (current_digits - strikes) + balls);
            }

            int outs = current_digits - (strikes + balls); // 아웃 계산
            print_result(strikes, balls, outs, 0); // 홈런은 별도로 표시
            led_on(strikes, balls, outs, (strikes == current_digits));

            if (strikes == current_digits) {
                writeToDotDevice(patterns[2], 2000000); // 홈런 패턴 표시
            }
            else {
                if (strikes > 0) {
                    writeToDotDevice(patterns[0], 2000000); // 스트라이크 패턴 표시
                }
                if (balls > 0) {
                    writeToDotDevice(patterns[3], 2000000); // 볼 패턴 표시
                }
                if (outs > 0) {
                    writeToDotDevice(patterns[1], 2000000); // 아웃 패턴 표시
                }
            }

            // 턴 변경
            turn = turn == 1 ? 2 : 1;
        }

        display_score(score1, score2);
    }

    print_final_score(score1, score2);
    char final_scores[5];
    sprintf(final_scores, "%04d", score1);
    PrintFnd(final_scores);
}

// 게임 소개 함수
int intro() {
    int dip_value = 0;

    print_clcd("DIP UP to Start");
    dip_value = dipsw_get_with_timer(100);  // 10초로 증가
    return dip_value;
}

// 메인 함수
int main() {
    init_devices();
    if (intro() != 0) {
        print_game_start();
        start_game();
        print_game_over();
    }
    else {
        print_clcd("Game Not Started");
        usleep(2000000);
    }
    return 0;
}
