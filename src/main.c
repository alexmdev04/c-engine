// SWADE: swade wont always do everything
// SOOP: sacrifice object oriented programming

#include "src.c"

int main(void) {
    printf("Here we go!\n");
    
    App app = SX_App_Init();
    // SX_App_Start(&app);
    SX_App_Loop(&app);
    SX_App_Stop(&app);
    return 0;
}