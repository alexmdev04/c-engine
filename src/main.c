// SWADE: swade wont always do everything
// SOOP: sacrifice object oriented programming

#include "src.c"

int main(i32 argc, c_str_arr argv) {
    printf("Here we go!\n\n");

    App app = SX_App_Init(argc, argv);
    // SX_App_Start(&app);
    SX_App_Update(&app);
    SX_App_Stop(&app);
    return 0;
}