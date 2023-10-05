#include<stdio.h>
#include"SDL2/SDL.h"
#include<stdlib.h> //exit()
#include<stdint.h>

//sdl container object
typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
}sdl_t;//sdl stuff

//sdl configuration object
typedef struct {
    uint32_t foreground_color;
    uint32_t background_color;
    uint32_t window_width; // sdl window width
    uint32_t window_height;// sdl window height
    uint32_t scale_factor; // scale original chip8 pixel e.g 20x


}config_t;//all configuration attributes, easy for tracking

// emulator states
typedef enum{
    QUIT,
    RUNNING,
    PAUSED,
}emulator_state_t;

//chip8 machine object
typedef struct {
    emulator_state_t state;
    uint8_t ram[4096];          //4K memory of chip8
    SDL_bool display[64*32];    //Emulate original chip8 resolution pixels
    uint8_t V[16];              //Data register V0~VF
    uint16_t I ;                //Index register
    uint16_t PC;                //Program counter register
    uint8_t delay_timer;        //Decrement at 60hz when>0
    uint8_t audio_timer;        //Decrement at 60hz and play music when>0
    SDL_bool keypad[16];        //Hex keypad 0x0-0xF
    char* rom_name;             //Currently running rom
}chip8_t;

//set clear screen to background color
void clear_screen(const config_t config, sdl_t sdl){
    const uint8_t r = (uint8_t)(config.background_color>>24)&0xFF;
    const uint8_t g = (uint8_t)(config.background_color>>16)&0xFF;
    const uint8_t b = (uint8_t)(config.background_color>>8)&0xFF;
    const uint8_t a = (uint8_t)(config.background_color>>0)&0xFF;

    SDL_SetRenderDrawColor(sdl.renderer,r,g,b,a); //set to background color
    SDL_RenderClear(sdl.renderer);
}

SDL_bool init_sdl(sdl_t* sdl,const config_t* config){
    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|SDL_INIT_AUDIO)!=0){
        SDL_Log("SDL subsystems initialize fail! %s\n",SDL_GetError());
        return SDL_FALSE;//init fail
    }
    //create window
    sdl->window = SDL_CreateWindow("chip8",SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED,
                                           config->window_width*config->scale_factor,
                                           config->window_height*config->scale_factor,
                                           0);
    if(sdl->window == NULL) {
        SDL_Log("Could not create SDL window! %s\n",SDL_GetError());
        return SDL_FALSE;//create window fail
    }
    //create renderer
    sdl->renderer = SDL_CreateRenderer(sdl->window,-1,SDL_RENDERER_ACCELERATED);
    if(sdl->renderer == NULL){
        SDL_Log("Could not create SDL Renderer! %s\n",SDL_GetError());
        return SDL_FALSE;//create Renderer fail
    }

    return SDL_TRUE;//init success
}

SDL_bool init_chip8(chip8_t* chip8, const char rom_name[]){
    const uint32_t entry_point = 0x200; //CHIP8 rom will be loaded to 0x200
    const uint8_t font[] = {
	0xF0, 0x90, 0x90, 0x90, 0xF0,		// 0    11110000
	0x20, 0x60, 0x20, 0x20, 0x70,		// 1    10010000
	0xF0, 0x10, 0xF0, 0x80, 0xF0,		// 2    10010000
	0xF0, 0x10, 0xF0, 0x10, 0xF0,		// 3    11110000
	0x90, 0x90, 0xF0, 0x10, 0x10,		// 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0,		// 5    11110000
	0xF0, 0x80, 0xF0, 0x90, 0xF0,		// 6    10000000
	0xF0, 0x10, 0x20, 0x40, 0x40,		// 7    11110000
	0xF0, 0x90, 0xF0, 0x90, 0xF0,		// 8    00010000
	0xF0, 0x90, 0xF0, 0x10, 0xF0,		// 9    11110000
	0xF0, 0x90, 0xF0, 0x90, 0x90,		// A
	0xE0, 0x90, 0xE0, 0x90, 0xE0,		// B
	0xF0, 0x80, 0x80, 0x80, 0xF0,		// C
	0xE0, 0x90, 0x90, 0x90, 0xE0,		// D
	0xF0, 0x80, 0xF0, 0x80, 0xF0,		// E
	0xF0, 0x80, 0xF0, 0x80, 0x80		// F
};
    // load font
    memcpy(&chip8->ram[0],font,sizeof(font)); //load font to the ram[0];

    // Open Rom file
    FILE* rom = fopen(rom_name, "rb");
    if(!rom){
        SDL_Log("Rom file %s can't not found or doesn't exist\n", rom_name);
        return SDL_FALSE;
    }
    // Get/check rom size
    fseek(rom,0,SEEK_END);              //pointer go to end of the file
    const size_t rom_size = ftell(rom); //tell the pointer position
    const size_t max_rom_size = sizeof(chip8->ram) - entry_point;
    rewind(rom);                        //pointer go back to the begining
    if (rom_size>max_rom_size){
        SDL_Log("ROM file %s is too large! Rom size: %zu , MAX size allowed: %zu\n",
                rom_name,rom_size,max_rom_size);
    }

    //read rom into chip8 ram
    if(fread(&chip8->ram[entry_point], rom_size, 1, rom)!=1){
       SDL_Log("Could not read rom:%s into chip8 memory\n",rom_name); 
    }; 
    
    fclose(rom);//close rom

    chip8->state = RUNNING;     //chip8 default on/running
    chip8->PC = entry_point;    //Program counter start at rom entry point
    return SDL_TRUE;            //init chip8 success
}


SDL_bool set_config(config_t* config,const int argc,char** argv){
    
    //set default config
    *config = (config_t){
        .window_width = 64,
        .window_height = 32,
        .foreground_color = 0xFFFFFFFF,//RGBA (white)
        .background_color = 0XFFFF00FF,//RGBA (yello)
        .scale_factor = 20, //Default resolution will be 1280*640 
    };

    //override default config
    for(int i=1;i<argc;i++){
        (void)argv[i];//prevent compile error
    }
    return SDL_TRUE;//set_config success.
}
void final__cleanup(const sdl_t sdl){
    SDL_DestroyWindow(sdl.window);
    SDL_DestroyRenderer(sdl.renderer);
    SDL_Quit();//shut down SDL subsystems
}

//update screen with any changes
void updatescreen(const sdl_t sdl){
    SDL_RenderPresent(sdl.renderer); 
}

//handle user input
void handle_input(chip8_t* chip8){
    SDL_Event event;
    while(SDL_PollEvent(&event)){
        switch (event.type){
            case SDL_QUIT://EXIT window END program
                chip8->state = QUIT; // Will exit the main emulator loop
                return;
            case SDL_KEYDOWN: //press the key
                switch (event.key.keysym.sym){//press the esc EXIT window END program
                    case SDLK_ESCAPE:
                        chip8->state = QUIT;
                        return;
                    default:
                        break; //default do nothing
                }
                break;
            case SDL_KEYUP: //loose the key
            default:
                break;
        }
    }
}
int main(int argc, char **argv){
    
    //Initialize Config
    config_t config = {0};
    set_config(&config,argc,argv);

    //Initialize SDL
    sdl_t sdl = {0};
    if(!init_sdl(&sdl,&config)) exit(EXIT_FAILURE);

    //Initial screen clear to background color;
    clear_screen(config,sdl);

    //Initialize chip8 machine
    chip8_t chip8 = {0};
    if(!init_chip8(&chip8, "test")) exit(EXIT_FAILURE);

    //Emulator main loop
    while(chip8.state!=QUIT){
        
        //handle user input
        handle_input(&chip8);

        //Delay 60hz = 16.7ms
        SDL_Delay(16);

        //update window with changes
        updatescreen(sdl);
    }
    //Final cleanup
    final__cleanup(sdl);
    exit(EXIT_SUCCESS);
}





