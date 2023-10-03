#include<stdio.h>
#include"SDL2/SDL.h"
#include<stdlib.h> //exit()
#include<stdbool.h> //bool, false, true

bool init_sdl(void){
    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|SDL_INIT_AUDIO)!=0){
        SDL_Log("SDL subsystems initialize fail!%s\n",SDL_GetError());
        return false;
    }
    return true;
}

int main(int argc, char **argv){
    puts("hello world");
    (void) argc;
    (void) argv;
    //Initialize SDL
    if(!init_sdl()) exit(EXIT_FAILURE);
    //Final cleanup
}





