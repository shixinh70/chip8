#include<stdio.h>
#include"SDL.h"
#include<stdlib.h> //exit()
#include<stdint.h>
#include<stdbool.h>
#include<time.h>
#ifdef DEBUG
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) ((void)0) 
#endif

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
    uint32_t instructions_per_second;       // chip8 cpu clock hz

}config_t;//all configuration attributes, easy for tracking

// emulator states
typedef enum{
    QUIT,
    RUNNING,
    PAUSED,
}emulator_state_t;

//CHIP8 Instruction format
typedef struct{
    uint16_t opcode;
    uint16_t NNN;       //12bits Address
    uint8_t NN;         //8bits Constant
    uint8_t N;          //4bits Constant
    uint8_t X;          //4bits Register ID
    uint8_t Y;          //4bits Register ID
    //uint8_t category;   // To category intructions
} intstruction_t;

//chip8 machine object
typedef struct {
    emulator_state_t state;
    uint8_t ram[4096];          //4K memory of chip8
    bool display[64*32];    //Emulate original chip8 resolution pixels
    uint16_t stack[16];         //CHIP8 subroutine stack
    uint16_t* stack_ptr;        //For use stack_ptr ++
    uint8_t V[16];              //Data register V0~VF
    uint16_t I ;                //Index register
    uint16_t PC;                //Program counter register
    uint8_t delay_timer;        //Decrement at 60hz when>0
    uint8_t audio_timer;        //Decrement at 60hz and play music when>0
    bool keypad[16];        //Hex keypad 0x0-0xF
    const char *rom_name;       //Currently running rom
    intstruction_t inst;        //Currently executing instruction
}chip8_t;




//set clear screen to background color
void init_screen(const config_t config, sdl_t sdl){
    //RGBA will be 32bits, each 8bits represent R, G, B, A
    //Take out each r, g ,b ,a value; 
    const uint8_t r = (uint8_t)(config.background_color>>24)&0xFF;
    const uint8_t g = (uint8_t)(config.background_color>>16)&0xFF;
    const uint8_t b = (uint8_t)(config.background_color>>8)&0xFF;
    const uint8_t a = (uint8_t)(config.background_color>>0)&0xFF;

    SDL_SetRenderDrawColor(sdl.renderer,r,g,b,a); //set to background color
    SDL_RenderClear(sdl.renderer);
}

bool init_sdl(sdl_t* sdl,const config_t* config){
    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|SDL_INIT_AUDIO)!=0){
        SDL_Log("SDL subsystems initialize fail! %s\n",SDL_GetError());
        return false;//init fail
    }
    //create window
    sdl->window = SDL_CreateWindow("chip8",SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED,
                                           config->window_width*config->scale_factor,
                                           config->window_height*config->scale_factor,
                                           0);
    if(sdl->window == NULL) {
        SDL_Log("Could not create SDL window! %s\n",SDL_GetError());
        return false;//create window fail
    }
    //create renderer
    sdl->renderer = SDL_CreateRenderer(sdl->window,-1,SDL_RENDERER_ACCELERATED);
    if(sdl->renderer == NULL){
        SDL_Log("Could not create SDL Renderer! %s\n",SDL_GetError());
        return false;//create Renderer fail
    }

    return true;//init success
}

bool init_chip8(chip8_t* chip8, const char rom_name[]){
    const uint32_t entry_point = 0x200; //CHIP8 rom will be loaded to 0x200
    const uint8_t font[] = {
	0xF0, 0x90, 0x90, 0x90, 0xF0,		// 0    11110000
	0x20, 0x60, 0x20, 0x20, 0x70,		// 1    1  10000
	0xF0, 0x10, 0xF0, 0x80, 0xF0,		// 2    1  10000
	0xF0, 0x10, 0xF0, 0x10, 0xF0,		// 3    1  10000
	0x90, 0x90, 0xF0, 0x10, 0x10,		// 4    11110000
	0xF0, 0x80, 0xF0, 0x10, 0xF0,		// 5    11110000
	0xF0, 0x80, 0xF0, 0x90, 0xF0,		// 6    1   0000
	0xF0, 0x10, 0x20, 0x40, 0x40,		// 7    11110000
	0xF0, 0x90, 0xF0, 0x90, 0xF0,		// 8       10000
	0xF0, 0x90, 0xF0, 0x10, 0xF0,		// 9    11110000
	0xF0, 0x90, 0xF0, 0x90, 0x90,		// A
	0xE0, 0x90, 0xE0, 0x90, 0xE0,		// B
	0xF0, 0x80, 0x80, 0x80, 0xF0,		// C
	0xE0, 0x90, 0x90, 0x90, 0xE0,		// D
	0xF0, 0x80, 0xF0, 0x80, 0xF0,		// E
	0xF0, 0x80, 0xF0, 0x80, 0x80		// F
};
    // Load Font
    memcpy(&chip8->ram[0],font,sizeof(font)); //load font to the ram[0];

    // Open Rom file
    FILE* rom = fopen(rom_name, "rb");
    if(!rom){
        SDL_Log("Rom file %s can't not found or doesn't exist\n", rom_name);
        return false;
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
    chip8->rom_name = rom_name;
    chip8->stack_ptr = &chip8->stack[0]; 
    return true;            //init chip8 success
}


bool set_config(config_t* config,const int argc,char** argv){
    
    //set default config
    *config = (config_t){
        .window_width = 64,
        .window_height = 32,
        .foreground_color = 0xFFFFFFFF,//RGBA (white)
        .background_color = 0X000000FF,//RGBA (black)
        .scale_factor = 20, //Default resolution will be 1280*640 
        .instructions_per_second = 700, // 1 second chip 8 fetch how much instructions
    };

    //override default config
    for(int i=1;i<argc;i++){
        (void)argv[i];//prevent compile error
    }
    return true;//set_config success.
}
void final__cleanup(const sdl_t sdl){
    SDL_DestroyWindow(sdl.window);
    SDL_DestroyRenderer(sdl.renderer);
    SDL_Quit();//shut down SDL subsystems
}

//update screen with any changes
void updatescreen(const sdl_t sdl, config_t config, const chip8_t* chip8 ){
    //Initialize SDL_Rect (draw a bit as a rectangle)
    SDL_Rect rect = {.x = 0, .y = 0, .w = config.scale_factor, .h = config.scale_factor};

    const uint8_t bg_r = (uint8_t)(config.background_color>>24)&0xFF;
    const uint8_t bg_g = (uint8_t)(config.background_color>>16)&0xFF;
    const uint8_t bg_b = (uint8_t)(config.background_color>>8)&0xFF;
    const uint8_t bg_a = (uint8_t)(config.background_color>>0)&0xFF;

    const uint8_t fg_r = (uint8_t)(config.foreground_color>>24)&0xFF;
    const uint8_t fg_g = (uint8_t)(config.foreground_color>>16)&0xFF;
    const uint8_t fg_b = (uint8_t)(config.foreground_color>>8)&0xFF;
    const uint8_t fg_a = (uint8_t)(config.foreground_color>>0)&0xFF;

    //Loops all the display pixels
    for (uint32_t i=0;i<sizeof(chip8->display);i++){
        //Translate index to 2D x,y
        rect.x = (i % config.window_width) * config.scale_factor;
        rect.y = (i / config.window_width) * config.scale_factor;

        //If display[i] is on, show foreground color
        //else draw background color
        if(chip8->display[i]){
            SDL_SetRenderDrawColor(sdl.renderer,fg_r, fg_g, fg_b, fg_a);
            SDL_RenderFillRect(sdl.renderer,&rect);
        }else{
            SDL_SetRenderDrawColor(sdl.renderer,bg_r, bg_g, bg_b, bg_a);
            SDL_RenderFillRect(sdl.renderer,&rect);
        }

        //Draw ouline of every forground pixel
        SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
        SDL_RenderDrawRect(sdl.renderer, &rect);
    }
    SDL_RenderPresent(sdl.renderer); 
}

//handle user input
//chip8's key
//123C      1234
//456D      qwer     
//789E      asdf
//A0BF      zxcv

void handle_input(chip8_t* chip8){
    SDL_Event event;
    while(SDL_PollEvent(&event)){
        switch (event.type){
            case SDL_QUIT://Press the cross or ALT+F4
                chip8->state = QUIT; // Will exit the main emulator loop
                return;
            case SDL_KEYDOWN: //press the key
                switch (event.key.keysym.sym){//Key symbol

                    case SDLK_ESCAPE:
                        chip8->state = QUIT;
                        return;

                    case SDLK_SPACE: //Space for pause/resume
                        if(chip8->state == RUNNING){
                            chip8->state = PAUSED; //PAUSE
                            puts("======= EMULATOR PAUSE =======");
                        }else{
                            chip8->state = RUNNING;//Resume
                        }
                        return;
                    case SDLK_1:chip8->keypad[0X1] = true; break;
                    case SDLK_2:chip8->keypad[0X2] = true; break;
                    case SDLK_3:chip8->keypad[0X3] = true; break;
                    case SDLK_4:chip8->keypad[0XC] = true; break;

                    case SDLK_q:chip8->keypad[0X4] = true; break;
                    case SDLK_w:chip8->keypad[0X5] = true; break;
                    case SDLK_e:chip8->keypad[0X6] = true; break;
                    case SDLK_r:chip8->keypad[0XD] = true; break;

                    case SDLK_a:chip8->keypad[0X7] = true; break;
                    case SDLK_s:chip8->keypad[0X8] = true; break;
                    case SDLK_d:chip8->keypad[0X9] = true; break;
                    case SDLK_f:chip8->keypad[0XE] = true; break;

                    case SDLK_z:chip8->keypad[0XA] = true; break;
                    case SDLK_x:chip8->keypad[0X0] = true; break;
                    case SDLK_c:chip8->keypad[0XB] = true; break;
                    case SDLK_v:chip8->keypad[0XF] = true; break;
                    default: break; //default do nothing
                }
                break;

            case SDL_KEYUP: //loose the key
                switch (event.key.keysym.sym){
                    case SDLK_1:chip8->keypad[0X1] = false; break;
                    case SDLK_2:chip8->keypad[0X2] = false; break;
                    case SDLK_3:chip8->keypad[0X3] = false; break;
                    case SDLK_4:chip8->keypad[0XC] = false; break;

                    case SDLK_q:chip8->keypad[0X4] = false; break;
                    case SDLK_w:chip8->keypad[0X5] = false; break;
                    case SDLK_e:chip8->keypad[0X6] = false; break;
                    case SDLK_r:chip8->keypad[0XD] = false; break;
                   
                    case SDLK_a:chip8->keypad[0X7] = false; break;
                    case SDLK_s:chip8->keypad[0X8] = false; break;
                    case SDLK_d:chip8->keypad[0X9] = false; break;
                    case SDLK_f:chip8->keypad[0XE] = false; break;
                   
                    case SDLK_z:chip8->keypad[0XA] = false; break;
                    case SDLK_x:chip8->keypad[0X0] = false; break;
                    case SDLK_c:chip8->keypad[0XB] = false; break;                   
                    case SDLK_v:chip8->keypad[0XF] = false; break;
                    default: break;
                }

            default:
                break;
        }
    }
}
//Emulate 1 chip-8 intruction
void emulate_instruction(chip8_t* chip8, config_t* config){
    //Get next intuction(16bits big-endian) and translate to opcode
    //CHIP8 instruction is BIG-endian
    chip8->inst.opcode = (chip8->ram[chip8->PC])<<8| chip8->ram[chip8->PC+1]; 
    chip8->PC += 2 ; //Move to next opcode (but not exec)
    
    //Fill in intruction format, (Mask out useless bits)
    chip8->inst.NNN = chip8->inst.opcode & 0X0FFF;   //12bits
    chip8->inst.NN = chip8->inst.opcode & 0X00FF;    //8bits   
    chip8->inst.N = chip8->inst.opcode & 0X000F;     //4bits
    chip8->inst.X = (chip8->inst.opcode>>8) & 0X000F;//4bits
    chip8->inst.Y = (chip8->inst.opcode>>4) & 0X000F;//4bits
    //category instrutions by first 4 bits (0-9, A-F)
    int8_t category = (chip8->inst.opcode >>12) & 0X000F;
    
    // Emulate opcode
    DEBUG_PRINT("Address: 0x%04X, Opcode: 0x%04X, Description: ",chip8->PC-2,chip8->inst.opcode);
    switch (category){
        case 0x00:  //0___ Start with 0
            if(chip8->inst.NN == 0XE0){
                // 00E0: Clear screen
                DEBUG_PRINT("Clear screen\n");
                memset(&(chip8->display[0]),0,sizeof(chip8->display)); //Set display[] to 0

            }else if(chip8->inst.NN == 0XEE){
                // 00EE: Return subroutine
                DEBUG_PRINT("Return subroutine to address 0x%04X\n",*(chip8->stack_ptr-1));
                // Set PC to last address from subroutine stack (pop off the address from the stack)
                chip8->stack_ptr--; //move back to last (stack) address
                chip8->PC = *(chip8->stack_ptr);
            }else{
                DEBUG_PRINT("Unimplemented opcode\n");
            }
            break;
        
        case 0x01:
            //1NNN : Jump to address NNN
            DEBUG_PRINT("Jump to address NNN (0x%04X)\n",chip8->inst.NNN);
            chip8->PC = chip8->inst.NNN;
        break;


        case 0x02:
            // Call subroutine at NNN
            DEBUG_PRINT("Call subroutine at NNN\n");
            *chip8->stack_ptr = chip8->PC; // Store current address before jumping (Push the address on stack)
            chip8->stack_ptr ++ ;          // Move the pointer to next empty space
            chip8->PC = chip8->inst.NNN;   // Set PC to subroutine's address NNN 
                                           // then next loop will execute opcode from NNN
            
            break;
        case 0x03:
            DEBUG_PRINT("Check if V%X (%02X)== NN (%02X), skip next instruction,\n",
            chip8->inst.X, chip8->V[chip8->inst.X],chip8->inst.NN);
            // 0x3XNN: Check if VX == NN, if so, skip the next instuction.
            if(chip8->V[chip8->inst.X] == chip8->inst.NN){
                chip8->PC +=2;
            }
            break;

        case 0x04:
            DEBUG_PRINT("Check if V%X (%02X)!= NN (%02X), skip next instruction,\n",
            chip8->inst.X, chip8->V[chip8->inst.X],chip8->inst.NN);
            // 0x4XNN: Check if VX == NN, if so, skip the next instuction.
            if(chip8->V[chip8->inst.X] != chip8->inst.NN){
                chip8->PC +=2;
            }
            break;
        case 0x05:
            DEBUG_PRINT("Check if V%X (%02X)== V%X (%02X), skip next instruction,\n",
            chip8->inst.X, chip8->V[chip8->inst.X],chip8->inst.Y,chip8->V[chip8->inst.Y]);
            // 0x5XY0: Check if VX == VY, if so, skip the next instuction.
            if(chip8->V[chip8->inst.X] == chip8->V[chip8->inst.Y]){
                chip8->PC +=2;
            }
            break;
        case 0x06:
            // 6XNN: Set register[X] to NN
            DEBUG_PRINT("Set register V[%X] to NN (0x%02X)\n",chip8->inst.X,chip8->inst.NN);
            chip8->V[chip8->inst.X] = chip8->inst.NN;
            break;

        case 0x07:
            //7XNN: Add const NN to register VX
            DEBUG_PRINT("ADD register V[%X] by NN (0x%02X)\n",chip8->inst.X,chip8->inst.NN);
            chip8->V[chip8->inst.X] += chip8->inst.NN;
            break;

        case 0x08:
            
            switch(chip8->inst.N){
                case 0x0:
                    // 0x8XY0: Set register VX = VY
                    DEBUG_PRINT("SET V[%X] = V[%X](%02X)\n",
                    chip8->inst.X, chip8->inst.Y, chip8->V[chip8->inst.Y]);
                    //0x8XY0: Set register VX = VY
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y];   
                    break;
                case 0x1:
                    DEBUG_PRINT("SET V[%X] |= V[%X](%02X)\n Result: %02X",
                    chip8->inst.X, chip8->inst.Y, chip8->V[chip8->inst.Y], 
                    chip8->V[chip8->inst.X] | chip8->V[chip8->inst.Y]);
                    //0x8XY1: Set register VX |= VY
                    chip8->V[chip8->inst.X] |= chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = 0;   
                    break;
                case 0x2:
                    DEBUG_PRINT("SET V[%X] &= V[%X](%02X) Result: %02X\n",
                    chip8->inst.X,chip8->inst.Y,chip8->V[chip8->inst.Y],
                    chip8->V[chip8->inst.X] & chip8->V[chip8->inst.Y]);
                    //0x8XY2: Set register VX &= VY
                    chip8->V[chip8->inst.X] &= chip8->V[chip8->inst.Y];   
                    chip8->V[0xF] = 0;
                    break;
                case 0x3:
                    DEBUG_PRINT("SET V[%X] ^= V[%X](%02X) Result: %02X\n",
                    chip8->inst.X,chip8->inst.Y,chip8->V[chip8->inst.Y],
                    chip8->V[chip8->inst.X] ^ chip8->V[chip8->inst.Y]);
                    //0x8XY3: Set register VX ^= VY
                    chip8->V[chip8->inst.X] ^= chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = 0;   
                    break;
                case 0x4:
                    DEBUG_PRINT("SET V[%X](%02X) += V[%X](%02X), V[F] = %02X (1 if carry) Result: %02X\n",
                    chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y], 
                    ((uint16_t)chip8->V[chip8->inst.X] + (uint16_t)chip8->V[chip8->inst.Y] > 255),
                    chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y]); 
                    //0x8XY4: Set register VX += VY, set V[F] to 1 if carry(over 255).
                    
                    chip8->V[0xF] = ((uint16_t)(chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y]) > 255);
                    chip8->V[chip8->inst.X] += chip8->V[chip8->inst.Y];
                    break;
                case 0x5:
                    DEBUG_PRINT("SET V[%X](%02X) -= V[%X](%02X), V[F] = %02X (0 if borrow) Result: %02X\n",
                    chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y],
                    ((uint16_t)chip8->V[chip8->inst.X] < (uint16_t)chip8->V[chip8->inst.Y]),
                    chip8->V[chip8->inst.X] - chip8->V[chip8->inst.Y]); 
                    //0x8XY5: Set register VX -= VY set V[F] to 0 if there is a borrow
                    chip8->V[0xF] = (chip8->V[chip8->inst.X]>=chip8->V[chip8->inst.Y]);
                    chip8->V[chip8->inst.X] -= chip8->V[chip8->inst.Y];
                       
                    break;

                case 0x6:
                    DEBUG_PRINT("V[%X](%02X) >>= 1 Result: %02X",
                    chip8->inst.X, chip8->inst.Y, chip8->V[chip8->inst.X] >> 1);
                    //0x8XY6: Store the lsb of VX in VF and shift VX to right by 1
                    chip8->V[0XF] = chip8->V[chip8->inst.Y] & 1;  // Take the lst bits to VF
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] >> 1;   
                    break;   

                case 0x7:
                    DEBUG_PRINT("SET V[%X](%02X) = V[%X](%02X) - V[%X](%02X), V[F] = %02X (0 if borrow) Result: %02X\n",
                    chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y],
                    chip8->inst.X, chip8->V[chip8->inst.X], 
                    ((uint16_t)chip8->V[chip8->inst.X] <= (uint16_t)chip8->V[chip8->inst.Y]),
                    chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X]); 
                    //0x8XY7: Sets VX to VY - VX. VF is set to 0 when there's a borrow, and 1 when there is not.
                    chip8->V[0XF] = (chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y]);
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X ];
                    break;   
                case 0xE:
                    DEBUG_PRINT("V[%X](%02X) <<= 1 Result: %02X",
                    chip8->inst.X, chip8->inst.Y, chip8->V[chip8->inst.X] << 1);
                    //0x8XYE: Set register VX <<= 1, store msb in VF
                    //VF is 8bit, so the msb will be VF & 2^7
                    chip8->V[0XF] = (chip8->V[chip8->inst.Y] & 0x80)>>7; //store msb in VF
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] << 1; //Set register VX <<= 1
                    break;   

                default:
                    break;
            }
            // 0X8XY1
            // 0X8XY2
            break;
        case 0X09:
            //Skips the next instruction if VX does not equal VY. 
            //(Usually the next instruction is a jump to skip a code block);
            DEBUG_PRINT("Check if V%X (%02X)!= V%X (%02X), skip next instruction,\n",
            chip8->inst.X, chip8->V[chip8->inst.X],chip8->inst.Y,chip8->V[chip8->inst.Y]);
            if(chip8->V[chip8->inst.X]!=chip8->V[chip8->inst.Y]){
                chip8->PC +=2;
            }
            break;
        case 0X0A:
            // ANNN: Set index register (I) to NNN
            DEBUG_PRINT("Set I to NNN (0x%04X)\n", chip8->inst.NNN);
            chip8->I = chip8->inst.NNN;
            break;
        case 0X0B:
            // BNNN: Jumps to the address NNN plus V0.
            DEBUG_PRINT("Jumps to NNN(0x%04X) + V[0](%02X) Result:%04X \n",
            chip8->inst.NNN,chip8->V[0],chip8->inst.NNN + chip8->V[0]);
            chip8->PC = chip8->inst.NNN + chip8->V[0];
            break;
        case 0X0C:
            // CXNN Sets VX to the result of a bitwise and operation on a random number (Typically: 0 to 255) and NN.
            DEBUG_PRINT("Set V[%X](%02X) to a (rand() %% 256) & NN(%X)\n",
            chip8->inst.X,chip8->V[chip8->inst.X],chip8->inst.NN);
            chip8->V[chip8->inst.X] = (rand() % 256) & chip8->inst.NN;
            break;    
        case 0X0D:
            // DXYN: Draw a sprite which stored at I to I+7 (8bits), to (x,y) on display
            //       for N rolls(height)
            DEBUG_PRINT("Drawing %u lines sprites at V[%X](0x%02X),V[%X](0x%02X) from I (0x%04X)\n",
                    chip8->inst.N,chip8->inst.X,chip8->V[chip8->inst.X],chip8->inst.Y,chip8->V[chip8->inst.Y],chip8->I);
            chip8->V[0XF] = 0; //Initial VF to 0 (Set to 1 when collision)
            uint8_t x = (chip8->V[chip8->inst.X] % config->window_width); // Clipped the over the monitor width
            uint8_t y = (chip8->V[chip8->inst.Y] % config->window_height);// Clipped the over the monitor height
            const uint8_t original_x = x; //Store the start x point
            //Loop N lines in constant N
            for(uint8_t i = 0;i < chip8->inst.N ;i++){
                //Get next bytes/row of sprite data (but not to increment I)
                const uint8_t sprite_data  = chip8->ram[chip8->I+i];
                x = original_x;//Reset x
                //Check if sprite data and display data was collision
                for(int8_t j = 7;j>=0;j--){
                    //Stop drawing if X hit the right edge of the screen

                    bool* display_xy_pixel = &(chip8->display[y*config->window_width + x]);
                    const bool sprite_bit = (sprite_data & (1<<j));
                    //If collision (sprite_data==1 , and display's (x,y) pixel ==1)
                    //then set the VF flag to 1
                    if(sprite_bit && (*display_xy_pixel)){
                        chip8->V[0XF] = 1;
                    } 
                    //Flipped the display' (x,y) pixel
                    *display_xy_pixel ^= sprite_bit;
                    //x has been mod by width, so it at least will be width -1
                    //must print one time, so check the edge at the end of the function.
                    //If next x over the edge, then stop drawing
                    if(++x >= config->window_width) break; 
                }
                if(++y >= config->window_height) break; //So does y
            }
    
            break;//break switch case(0x0D)

        case 0X0E:
            switch (chip8->inst.NN){
                case 0x9E:
                    //EX9E: Skips the next instruction if the key stored in VX is pressed
                    DEBUG_PRINT("Skip next instrction if key in V[%X](0x%02X) is pressed; Keypad value is %d\n",
                                chip8->inst.X,chip8->V[chip8->inst.X],chip8->keypad[chip8->V[chip8->inst.X]]);
                    if(chip8->keypad[chip8->V[chip8->inst.X]])
                        chip8->PC += 2;
                    break;
                case 0XA1:
                    //EXA1: Skips the next instruction if the key stored in VX is not pressed
                    DEBUG_PRINT("Skip next instrction if key in V[%X](0x%02X) is not pressed; Keypad value is %d\n",
                                chip8->inst.X,chip8->V[chip8->inst.X],chip8->keypad[chip8->V[chip8->inst.X]]);
                    if(!chip8->keypad[chip8->V[chip8->inst.X]])
                        chip8->PC += 2;     
                    break;
            default:
                break;
            }     
            break; //break for 0XE
        case 0X0F:
            switch (chip8->inst.NN){
                case 0X07:
                    //FX07: Sets VX to the value of the delay timer.
                    DEBUG_PRINT("Set delay timer(%02X) to V[%X]\n",
                    chip8->delay_timer, chip8->inst.X);
                    chip8->V[chip8->inst.X] = chip8->delay_timer;
                    break;
                case 0X0A:{
                        //FX0A: Wait until key pressed, and store in VX.
                        bool key_pressed = false;
                        DEBUG_PRINT("Wait to a key pressed, then store into V[%X]\n",
                        chip8->V[chip8->inst.X]);
                        for(uint8_t i=0;i<sizeof(chip8->keypad);i++){
                            if(chip8->keypad[i]){
                                chip8->V[chip8->inst.X] = i; //i map to 0X0-0XF
                                key_pressed = true;
                                break;
                            }
                        }
                        //If no key pressed
                        //In order to run the same intruction but still refresh the window
                        //PC -=2, then the new round will refresh window then do current intruction 
                        if(!key_pressed){
                            chip8->PC -=2;
                        }
                        
                    }
                    break;
                case 0X15:  
                    //FX07: Sets delay timer to VX .
                    DEBUG_PRINT("Set V[%X](%02X) to delay timer\n",
                    chip8->inst.X,chip8->V[chip8->inst.X]);
                    chip8->delay_timer = chip8->V[chip8->inst.X];
                break;
                case 0X18:
                    //FX07: Sets sound timer to VX .
                    DEBUG_PRINT("Set V[%X](%02X) to sound timer\n",
                    chip8->inst.X,chip8->V[chip8->inst.X]);
                    chip8->audio_timer = chip8->V[chip8->inst.X];
                break;
                case 0X1E:
                    //FX1E: I += VX; 
                    DEBUG_PRINT("I(0x%04X) += V[%X](0x%02X), Result:0x%04X",
                    chip8->I,chip8->inst.X,chip8->V[chip8->inst.X], chip8->I + chip8->V[chip8->inst.X]);
                    chip8->I += chip8->V[chip8->inst.X];
                break;
                case 0X29:
                    DEBUG_PRINT("Set the I to the font store in V[%X](%02X), which is %04X\n",
                    chip8->inst.X, chip8->V[chip8->inst.X], chip8->V[chip8->inst.X] * 5);
                    //FX29: Sets I to the location of the sprite for the character in VX.
                    //The start address of the character (Since I store the font 1 to F at the memory[0])
                    //So the font of V[X]'s address will be V[X] *5 (Each font contains 5 rows) 
                    chip8->I = chip8->V[chip8->inst.X] * 5; 

                break;
                case 0X33:{
                    DEBUG_PRINT("Stores the binary-coded decimal representation of VX\n");
                    //FX33 Stores the binary-coded decimal representation of VX,
                    //with the hundredsu digit in memory at location in I, 
                    //the tens digit at location I+1, and the ones digit at location I+2
                    uint8_t tmp = chip8->V[chip8->inst.X];
                    chip8->ram[chip8->I+2] = tmp%10; //the ones digit
                    tmp/=10;
                    chip8->ram[chip8->I+1] = tmp%10; //the tens digit
                    tmp/=10;
                    chip8->ram[chip8->I+0] = tmp; //the hundred digit
                }
                break;
                case 0X55:
                    DEBUG_PRINT("Stores V[0] to V[%X] from I(%04X) to I(%04X)\n",
                    chip8->I, chip8->inst.X ,chip8->I + chip8->inst.X);
                    //FX55 Stores from V0 to VX (including VX) in memory (I+0 - I+X), I itself unmodified.
                    for(uint8_t i=0;i<=chip8->inst.X;i++)
                        chip8->ram[chip8->I++] = chip8->V[i];
                    
                break;
                case 0X65:
                    DEBUG_PRINT("Load V[0] to V[%X] from I(%04X) to I(%04X)\n",
                    chip8->I, chip8->inst.X ,chip8->I + chip8->inst.X);
                    //FX65 Load  V0 to VX (including VX) from (I+0 - I+X), I itself unmodified.
                    for(uint8_t i=0;i<=chip8->inst.X;i++)
                        chip8->V[0+i] = chip8->ram[chip8->I++];
                break;
                default:
                break;
            }
        break;
            
        default:
            DEBUG_PRINT("Unimplemented opcode\n");
            break; //Unimplemented opcode or error opcode

    }
}

void update_chip8_timer(chip8_t* chip8){
    if(chip8->delay_timer>0)
        chip8->delay_timer --;
    if(chip8->audio_timer>0)
        chip8->audio_timer --;
        
};
int main(int argc, char **argv){
    
    // Uasage message for miss args
    if(argc<2){
        fprintf(stderr,"Usage: %s <rom_name>\n",argv[0]);// Usage ./chip <rome_name>
        
    }
    //Initialize Config
    config_t config = {0};
    set_config(&config,argc,argv);//Set some default config

    //Initialize SDL
    sdl_t sdl = {0};
    if(!init_sdl(&sdl,&config)) exit(EXIT_FAILURE);

    //Initial screen clear to background color;
    init_screen(config,sdl);

    //Initialize chip8 machine like ./chip8 rom_name
    chip8_t chip8 = {0};
    const char* rom_name = argv[1];
    if(!init_chip8(&chip8, rom_name)) exit(EXIT_FAILURE); 

    //Initialize rand function with time seed 
    srand(time(NULL));
    

    //Get time()
    //Emulator main loop
    while(chip8.state!=QUIT){
        
        //handle user input
        handle_input(&chip8);
        
        if (chip8.state == PAUSED) continue;

        
        //Get time before instructions
        //Since some intrcution may take longer time to proccess(like drawing the picture),
        //SO the delay time should be dynamic (along with the intruction hadling time per frame ) 
        uint64_t start_instructions_counts = SDL_GetPerformanceCounter();
        

        // If I want to cpu process n intructions/seconds, and we refresh display every second 1/60.
        // so every frame we need to do n/60 instructions.
        for(uint32_t i = 0; i< config.instructions_per_second / 60;i++){
            emulate_instruction(&chip8,&config);

        } 

        //Get time after instructions
        uint64_t end_instructions_counts = SDL_GetPerformanceCounter();


        double instructions_cost_ms =
        (double)(end_instructions_counts- start_instructions_counts) 
         / SDL_GetPerformanceFrequency() * 1000; //to ms
        
        //Delay 60hz = 16.7ms
        //If instrctions time cost has been over the 16.7, than don't delay , else complment to 16.67  
        SDL_Delay((double)16.67 >= instructions_cost_ms ? (double)16.67 - instructions_cost_ms : 0);

        //update window with changes (60hz)
        updatescreen(sdl, config, &chip8);
        update_chip8_timer(&chip8);
    }
    //Final cleanup
    final__cleanup(sdl);
    exit(EXIT_SUCCESS);
}





