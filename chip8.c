#include<stdio.h>
#include"SDL2/SDL.h"
#include<stdlib.h> //exit()
#include<stdbool.h> //bool, false, true
#include<stdint.h>
typedef struct {
    SDL_Window *window;
}sdl_t;//sdl stuff

typedef struct {
    uint32_t window_width; // sdl window width
    uint32_t window_height;// sdl window height
    uint32_t window_w;

}config_t;//all configuration attributes, easy for tracking


bool init_sdl(sdl_t* sdl,const config_t* config){
    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|SDL_INIT_AUDIO)!=0){
        SDL_Log("SDL subsystems initialize fail! %s\n",SDL_GetError());
        return false;//init fail
    }
    //create window
    sdl->window = SDL_CreateWindow("chip8",SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED,
                                           config->window_width,
                                           config->window_height,0);
    if(sdl->window == NULL) {
        SDL_Log("Could not create SDL window! %s\n",SDL_GetError());
        return false;//create window fail
    }
    return true;//init success
}
bool set_config(config_t* config,const int argc,char** argv){
    
    //set default config
    *config = (config_t){
        .window_width = 64,
        .window_height = 32,
    };

    //override default config
    for(int i=1;i<argc;i++){
        (void)argv[i];//prevent compile error
    }
    return true;//set_config success.
}
void final__cleanup(const sdl_t* sdl){
    SDL_DestroyWindow(sdl->window);
    SDL_Quit();//shut down SDL subsystems
}

int main(int argc, char **argv){
    (void) argc;//prevent compile error
    (void) argv;//prevent compile error

    //Initialize Config
    config_t config = {0};
    set_config(&config,argc,argv);

    //Initialize SDL
    sdl_t sdl = {0};
    if(!init_sdl(&sdl,&config)) exit(EXIT_FAILURE);



    //Final cleanup
    final__cleanup(&sdl);
}





