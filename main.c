/**********************************************************************/
//** Lab 02

//** Name: Joshua Miller
//** RIN:  662039082

//** Name: Philip Paterson
//** RIN:  662031059
/**********************************************************************/

#include "engr2350_msp432.h"


// Consts
typedef enum _RGB_LED_COLOR {
    RGB_LED_OFF = -1,
    RGB_LED_RED = 0,
    RGB_LED_GREEN = 1,
    RGB_LED_BLUE = 2,
    RGB_LED_MAGENTA = 3,
    RGB_LED_CYAN = 4,
    RGB_LED_YELLOW = 5
} RGB_LED_COLOR;

typedef enum _BILED_COLOR {
    BILED_OFF = -1,
    BILED_RED = 0,
    BILED_GREEN = 1
} BILED_COLOR;

uint8_t seed = 1;


// Initializations
void GPIO_Init();
void Timers_Init();
void Pattern_Init();


// Helper functions
void wait_seconds(uint8_t seconds);
void set_RGB_LED(RGB_LED_COLOR color);
void set_RGB_LED_by_BMP();
void set_BILED(BILED_COLOR color);
void show_pattern(uint8_t removed);


// State Functions
void show_colors_state();
void test_buttons_state();
void input_colors_state();
void game_done_state(BILED_COLOR color);


// Tests
void wait_5_seconds_test();
void RGB_LED_color_test();
void RGB_LED_by_BMP_test();
void BILED_color_test();

void pb_interrupt_test();
void bmp_interrupt_test();

void pattern_init_test();


// Interrupts
void timer50ms_interrupt();

void same_port_interrupt();
void bmp_interrupt();
void pb_interrupt();


// Pins
uint8_t RGB_LED_PORT        = GPIO_PORT_P2;
uint8_t RGB_LED_PINS[]      = {GPIO_PIN0, GPIO_PIN1, GPIO_PIN2};

uint8_t BILED_PORT          = GPIO_PORT_P3;
uint8_t BILED_PINS[]        = {GPIO_PIN2, GPIO_PIN3};

uint8_t PB_PORT_PIN[]       = {GPIO_PORT_P4, GPIO_PIN1};

uint8_t BMP_PORT            = GPIO_PORT_P4;
uint8_t BMP_PINS[]          = {GPIO_PIN0, GPIO_PIN2, GPIO_PIN3, GPIO_PIN5, GPIO_PIN6, GPIO_PIN7};


// Globals
uint8_t count_50ms; // How many 50ms have passed

Timer_A_UpModeConfig timerConfig50ms;

uint8_t pb_pressed;
int8_t bmp_pressed;

uint8_t state; // What program state we in?
uint8_t reminders_left;
uint8_t removed_patterns;

RGB_LED_COLOR color_pattern[5];


int main (void) /* Main Function */
{
    SysInit();

    GPIO_Init();
    Timers_Init();

    srand(seed);

    //pb_interrupt_test();

    while(1){ // Loop per game
        // Initialize variables
        pb_pressed = 0;
        bmp_pressed = -1;

        state = 0;
        reminders_left = 1;
        removed_patterns = 0;

        Pattern_Init();

        printf("playing instructions\r\n");

        while (1) { // Main Game Loop
            switch (state) { // What state we in?
            case 0: // Start of game, waits till pb pressed
                test_buttons_state();
                break;
            case 1: // Waiting 1 sec 'til game start
                wait_seconds(1);
                state = 2;
                break;
            case 2: // Blinky bloopy colors
                show_colors_state();
                break;
            case 3:
                input_colors_state();
                break;
            case 4:
                game_done_state(BILED_RED);
                break;
            case 5:
                game_done_state(BILED_GREEN);
                break;
            }
            if (state == 6) { // New Game
                set_BILED(BILED_OFF);
                break;
            }
        }
    }
}


// Init
void GPIO_Init() {
    // Input Output
    {
        // RGB LED Boi
        GPIO_setAsOutputPin(RGB_LED_PORT, RGB_LED_PINS[0] | RGB_LED_PINS[1] | RGB_LED_PINS[2]);
        set_RGB_LED(RGB_LED_OFF);

        // BILED Lad
        GPIO_setAsOutputPin(BILED_PORT, BILED_PINS[0] | BILED_PINS[1]);
        set_BILED(BILED_OFF);

        // Pushbutton
        GPIO_setAsInputPin(PB_PORT_PIN[0], PB_PORT_PIN[1]);

        // Bumpers
        GPIO_setAsInputPinWithPullUpResistor(BMP_PORT, BMP_PINS[0] | BMP_PINS[1] | BMP_PINS[2] | BMP_PINS[3] | BMP_PINS[4] | BMP_PINS[5]);
    }

    // Interrupt
    {
        GPIO_enableInterrupt(BMP_PORT, BMP_PINS[0] | BMP_PINS[1] | BMP_PINS[2] | BMP_PINS[3] | BMP_PINS[4] | BMP_PINS[5]);
        GPIO_interruptEdgeSelect(BMP_PORT, BMP_PINS[0] | BMP_PINS[1] | BMP_PINS[2] | BMP_PINS[3] | BMP_PINS[4] | BMP_PINS[5], GPIO_LOW_TO_HIGH_TRANSITION);

        GPIO_enableInterrupt(PB_PORT_PIN[0], PB_PORT_PIN[1]);
        GPIO_interruptEdgeSelect(PB_PORT_PIN[0], PB_PORT_PIN[1], GPIO_HIGH_TO_LOW_TRANSITION);

        if (BMP_PORT == PB_PORT_PIN[0]) { // If the port is the same for the two isrs
            GPIO_registerInterrupt(BMP_PORT, same_port_interrupt);
        } else { // Not the same, don't worry
            GPIO_registerInterrupt(BMP_PORT, bmp_interrupt);
            GPIO_registerInterrupt(PB_PORT_PIN[0], pb_interrupt);
        }
    }
}

void Timers_Init() {
    timerConfig50ms = (Timer_A_UpModeConfig){
        .clockSource = TIMER_A_CLOCKSOURCE_SMCLK,
        .clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_64,
        .timerPeriod = 18750, // Update
        .timerInterruptEnable_TAIE = TIMER_A_TAIE_INTERRUPT_ENABLE,
        .timerClear = TIMER_A_DO_CLEAR
    };

    Timer_A_configureUpMode(TIMER_A1_BASE, &timerConfig50ms);
    Timer_A_registerInterrupt(TIMER_A1_BASE, TIMER_A_CCRX_AND_OVERFLOW_INTERRUPT, timer50ms_interrupt);
    count_50ms = 0;
}

void Pattern_Init() {
    uint8_t i;
    for (i = 0; i < 5; i++) {
        color_pattern[i] = (RGB_LED_COLOR) (rand(seed)%6);
    }
}


// Helpers
void wait_seconds(uint8_t seconds){
    Timer_A_stopTimer(TIMER_A1_BASE);
    Timer_A_clearTimer(TIMER_A1_BASE);
    count_50ms = 0;
    Timer_A_startCounter(TIMER_A1_BASE, TIMER_A_UP_MODE);

    while (seconds*20 > count_50ms) {} // 1000ms/50ms

    Timer_A_stopTimer(TIMER_A1_BASE);
}

void set_RGB_LED(RGB_LED_COLOR color) {
    switch (color) {
    case RGB_LED_RED:
        GPIO_setOutputHighOnPin(RGB_LED_PORT, RGB_LED_PINS[0]);
        GPIO_setOutputLowOnPin(RGB_LED_PORT, RGB_LED_PINS[1]);
        GPIO_setOutputLowOnPin(RGB_LED_PORT, RGB_LED_PINS[2]);
        break;

    case RGB_LED_GREEN:
        GPIO_setOutputLowOnPin(RGB_LED_PORT, RGB_LED_PINS[0]);
        GPIO_setOutputHighOnPin(RGB_LED_PORT, RGB_LED_PINS[1]);
        GPIO_setOutputLowOnPin(RGB_LED_PORT, RGB_LED_PINS[2]);
        break;

    case RGB_LED_BLUE:
        GPIO_setOutputLowOnPin(RGB_LED_PORT, RGB_LED_PINS[0]);
        GPIO_setOutputLowOnPin(RGB_LED_PORT, RGB_LED_PINS[1]);
        GPIO_setOutputHighOnPin(RGB_LED_PORT, RGB_LED_PINS[2]);
        break;

    case RGB_LED_MAGENTA:
        GPIO_setOutputHighOnPin(RGB_LED_PORT, RGB_LED_PINS[0]);
        GPIO_setOutputLowOnPin(RGB_LED_PORT, RGB_LED_PINS[1]);
        GPIO_setOutputHighOnPin(RGB_LED_PORT, RGB_LED_PINS[2]);
        break;

    case RGB_LED_CYAN:
        GPIO_setOutputLowOnPin(RGB_LED_PORT, RGB_LED_PINS[0]);
        GPIO_setOutputHighOnPin(RGB_LED_PORT, RGB_LED_PINS[1]);
        GPIO_setOutputHighOnPin(RGB_LED_PORT, RGB_LED_PINS[2]);
        break;

    case RGB_LED_YELLOW:
        GPIO_setOutputHighOnPin(RGB_LED_PORT, RGB_LED_PINS[0]);
        GPIO_setOutputHighOnPin(RGB_LED_PORT, RGB_LED_PINS[1]);
        GPIO_setOutputLowOnPin(RGB_LED_PORT, RGB_LED_PINS[2]);
        break;
    default:
        GPIO_setOutputLowOnPin(RGB_LED_PORT, RGB_LED_PINS[0]);
        GPIO_setOutputLowOnPin(RGB_LED_PORT, RGB_LED_PINS[1]);
        GPIO_setOutputLowOnPin(RGB_LED_PORT, RGB_LED_PINS[2]);
    }
}

void set_RGB_LED_by_BMP() { // Whatever bmp is pressed is the color that is being colored
    if (!GPIO_getInputPinValue(BMP_PORT, BMP_PINS[0])) {
        set_RGB_LED(RGB_LED_RED);
    } else if (!GPIO_getInputPinValue(BMP_PORT, BMP_PINS[1])) {
        set_RGB_LED(RGB_LED_GREEN);
    } else if (!GPIO_getInputPinValue(BMP_PORT, BMP_PINS[2])) {
        set_RGB_LED(RGB_LED_BLUE);
    } else if (!GPIO_getInputPinValue(BMP_PORT, BMP_PINS[3])) {
        set_RGB_LED(RGB_LED_MAGENTA);
    } else if (!GPIO_getInputPinValue(BMP_PORT, BMP_PINS[4])) {
        set_RGB_LED(RGB_LED_CYAN);
    } else if (!GPIO_getInputPinValue(BMP_PORT, BMP_PINS[5])) {
        set_RGB_LED(RGB_LED_YELLOW);
    } else {
        set_RGB_LED(RGB_LED_OFF);
    }
}

void set_BILED(BILED_COLOR color) {
    switch (color) {
    case BILED_OFF:
        GPIO_setOutputLowOnPin(BILED_PORT, BILED_PINS[0]);
        GPIO_setOutputLowOnPin(BILED_PORT, BILED_PINS[1]);
        break;

    case BILED_RED:
        GPIO_setOutputHighOnPin(BILED_PORT, BILED_PINS[0]);
        GPIO_setOutputLowOnPin(BILED_PORT, BILED_PINS[1]);
        break;

    case BILED_GREEN:
        GPIO_setOutputLowOnPin(BILED_PORT, BILED_PINS[0]);
        GPIO_setOutputHighOnPin(BILED_PORT, BILED_PINS[1]);
        break;
    }
}

// State functions
void show_colors_state() {
    uint8_t pattern_on = 0; // Reset variables

    set_BILED(BILED_RED); // Tell it is showin colors

    Timer_A_stopTimer(TIMER_A1_BASE);

    while (pattern_on < 5-removed_patterns) { // While still colors to show
        set_RGB_LED(color_pattern[pattern_on]); // Set the led to the color

        Timer_A_clearTimer(TIMER_A1_BASE);
        count_50ms = 0;
        Timer_A_startCounter(TIMER_A1_BASE, TIMER_A_UP_MODE);


        while (20 > count_50ms) { // Still within blink time
            if (10 < count_50ms) { // Half way through blink (turn off)
                set_RGB_LED(RGB_LED_OFF);
            }
        }

        pattern_on++;
    }

    // State closing
    set_BILED(BILED_OFF);
    set_RGB_LED(RGB_LED_OFF);
    state = 3;
}

void test_buttons_state() {
    pb_pressed = 0; // Reset pushbutton just in case

    while (pb_pressed == 0) { // While the pb hasn't been pressed, show the bumper color
        set_RGB_LED_by_BMP();
    }

    // Button Pressed
    printf("Starting Game:\r\n"); // Maybe Temporary? Can we say it startin?
    state = 1;
}

void input_colors_state() {
    // Reset variables
    uint8_t pattern_on = 0;
    bmp_pressed = -1;
    pb_pressed = 0;
    Timer_A_stopTimer(TIMER_A1_BASE);

    set_BILED(BILED_GREEN); // Tell user it input time

    if (removed_patterns > 0 ) { // Not the first time?
        // Clear & restart timer (just in case)
        Timer_A_clearTimer(TIMER_A1_BASE);
        count_50ms = 0;
        Timer_A_startCounter(TIMER_A1_BASE, TIMER_A_UP_MODE);
        while (1) {
            if (reminders_left > 0) { // Still Have reminders?
                if (pb_pressed == 1) { // PB Pressed (requesting reminder)
                    reminders_left--;
                    state = 2;
                    break;
                }
            }


            if (count_50ms < 60) { // Still within time limit? (3000ms/50ms)
                if (bmp_pressed != -1) { // Button been fully pressed yet?
                    if (bmp_pressed == color_pattern[5-removed_patterns+pattern_on]) { // Correct button?
                        pattern_on++;
                        bmp_pressed = -1;

                        Timer_A_clearTimer(TIMER_A1_BASE);
                        count_50ms = 0;

                        if (pattern_on >= removed_patterns) { // we entered enough?
                            state = 2;
                            removed_patterns++;

                            if (removed_patterns > 5) { // We win?
                                printf("You won, congrats!\r\n");
                                state = 5;
                                break;
                            }

                            printf("Correct, waiting 5sec\r\n");
                            set_BILED(BILED_RED);
                            wait_seconds(5);
                            break;
                        }
                    } else {
                        printf("Incorrect\r\n");
                        state = 4;
                        break;
                    }
                } else {
                    set_RGB_LED_by_BMP();
                }
            } else {
                state = 4;
                printf("Took too long to enter\r\n");
                break;
            }
        }

    } else { // If there is no patters, we only wait 2 sec... thx
        set_BILED(BILED_RED);
        wait_seconds(2);
        state = 2;
        removed_patterns++;
    }

    // Ending state
    Timer_A_stopTimer(TIMER_A1_BASE);
    set_BILED(BILED_OFF);
    set_RGB_LED(RGB_LED_OFF);
}

void game_done_state(BILED_COLOR color) {
    pb_pressed = 0;

    Timer_A_clearTimer(TIMER_A1_BASE);
    count_50ms = 0;
    Timer_A_startCounter(TIMER_A1_BASE, TIMER_A_UP_MODE);

    while (pb_pressed == 0) { // 250ms/50ms
        if (count_50ms >= 2*5) { // If blink has passed subtract that time
            count_50ms -= 2*5;
        }

        if (count_50ms < 5) {
            set_BILED(color);
        } else {
            set_BILED(BILED_OFF);
        }
    }

    Timer_A_stopTimer(TIMER_A1_BASE);
    state = 6;
}


// Tests
void wait_5_seconds_test(){
    printf("Waiting 5 Seconds\r\n");
    wait_seconds(5);
    printf("5 Seconds have elapsed\r\n");
}

void RGB_LED_color_test() { // Go through the colors and test em
    RGB_LED_COLOR i;
    for (i = RGB_LED_OFF; i <= RGB_LED_YELLOW; i++) {
        printf("ENUM POS: %d\r\n",i);
        set_RGB_LED(i);

        // Crude delay
        uint32_t count;
        for(count=10000000; count>0; count--);
    }
}

void RGB_LED_by_BMP_test(){ // Just constantly show the color from bmp
    while (1) {
        set_RGB_LED_by_BMP();
    }
}

void BILED_color_test() { // Show red then green
    printf("RED\r\n");
    set_BILED(BILED_RED);

    // Crude delay
    uint32_t count;
    for(count=10000000; count>0; count--);

    printf("GREEN\r\n");
    set_BILED(BILED_GREEN);

    // Crude delay
    for(count=10000000; count>0; count--);

    printf("OFF\r\n");
    set_BILED(BILED_OFF);
}

void pb_interrupt_test() {
    printf("Waiting for PB\r\n");
    while (1) {
        if (pb_pressed) {
            printf("PB Pressed\r\n");
            pb_pressed = 0;
        }
    }
}

void bmp_interrupt_test() {
    while (1) {
        if (bmp_pressed != -1) {
            printf("BMP%d Pressed!", bmp_pressed);
            bmp_pressed = -1;
        }
    }
}

void pattern_init_test() {
    Pattern_Init();

    printf("Pattern is: [");
    uint8_t i;
    for (i = 0; i < 5; i++) {
        printf("%d", color_pattern[i]);

        if (i != 4) printf(", ");
    }
    printf("]\r\n");
}


// Interrupts
void timer50ms_interrupt() {
    Timer_A_clearInterruptFlag(TIMER_A1_BASE);
    count_50ms++;
}

void same_port_interrupt() {
    __delay_cycles(240e3); // Debounceage
    uint8_t active_pins = GPIO_getEnabledInterruptStatus(GPIO_PORT_P4);

    if(active_pins & PB_PORT_PIN[1]){ // Pushbutton?
        GPIO_clearInterruptFlag(PB_PORT_PIN[0],PB_PORT_PIN[1]);
        if(!GPIO_getInputPinValue(PB_PORT_PIN[0],PB_PORT_PIN[1])){
            pb_pressed = 1;
        }
    } else {
        uint8_t i;
        for (i = 0; i < 6; i++) {
            if(active_pins & BMP_PINS[i]){
                GPIO_clearInterruptFlag(BMP_PORT, BMP_PINS[i]);
                if(!GPIO_getInputPinValue(BMP_PORT,BMP_PINS[i])){
                    bmp_pressed = i;
                } else {
                    bmp_pressed = -1; // May need to remove
                }
            }
        }
    }
}

void bmp_interrupt() {
    __delay_cycles(240e3); // Debounceage
    uint8_t active_pins = GPIO_getEnabledInterruptStatus(GPIO_PORT_P4);

    uint8_t i;
    for (i = 0; i < 6; i++) {
        if(active_pins & BMP_PINS[i]){
            GPIO_clearInterruptFlag(BMP_PORT, BMP_PINS[i]);
            if(!GPIO_getInputPinValue(BMP_PORT,BMP_PINS[i])){
                bmp_pressed = i;
            } else {
                bmp_pressed = -1; // May need to remove
            }
        }
    }
}

void pb_interrupt() {
    __delay_cycles(240e3); // Debounceage
    uint8_t active_pins = GPIO_getEnabledInterruptStatus(PB_PORT_PIN[0]);

    if(active_pins & PB_PORT_PIN[1]){
        GPIO_clearInterruptFlag(PB_PORT_PIN[0],PB_PORT_PIN[1]);
        if(!GPIO_getInputPinValue(PB_PORT_PIN[0],PB_PORT_PIN[1])){
            pb_pressed = 1;
        }
    }
}
