#define LINUX_BUILD
//#define WINDOWS_BUILD

#ifdef LINUX_BUILD
    #include<SDL2/SDL.h>
    #include<SDL2/SDL_image.h>
    #include<SDL2/SDL_mixer.h>
#endif
#ifdef WINDOWS_BUILD
    #include<SDL2/SDL.h>
    #include<SDL2/SDL_image.h>
    #include<SDL2/SDL_mixer.h>
#endif

#include<stdio.h>
#include<math.h>
#include<time.h>

#define bool _Bool
#define true 1
#define false 0


typedef enum{
    up      = 0b1,
    down    = 0b10,
    left    = 0b100,
    right   = 0b1000,
}ControlState;

typedef enum{
    air,
    rock,
    hard_rock,
    dirt,
    grass
}TileType;

typedef enum{
    nothing,
    water,
    iron,
    coal,
    gold,
    rooted,
}ContentType;

typedef struct{
    Uint8 type;
    Uint8 content;
}Tile;

#define worldw 1000
#define worldh 1000

Tile world[worldw][worldh];

//global variables
bool running;

bool key_change;

#define key_pressed(key) (control & key)

Uint8 FPS,real_FPS;
Uint8 control;
Uint32 timer, total_time;

Sint32 SEED;


SDL_Point camera;
SDL_Point root;

//                      rock - hard rock - grass - dirt -       log    -   gold -   iron - water -    coal -        root    -  select
SDL_Rect textures[11] = {{0,0},{1*16,0}, {0,1*16},{1*16,1*16},{0,2*16},  {2*16,0},{3*16,0},{2*16,1*16},{3*16,1*16},{2*16,2*16},{1*16,2*16}};

SDL_Window*     window;
SDL_Renderer*   renderer;
SDL_Texture*    spritesheet,*font;
SDL_Surface     icon;

Uint8 volume = 64;
Mix_Music*   soundtrack; 


//credits for perlin noise: https://gist.github.com/nowl/828013
#pragma region perlin noise


static const unsigned char  HASH[] = {
    208,34,231,213,32,248,233,56,161,78,24,140,71,48,140,254,245,255,247,247,40,
    185,248,251,245,28,124,204,204,76,36,1,107,28,234,163,202,224,245,128,167,204,
    9,92,217,54,239,174,173,102,193,189,190,121,100,108,167,44,43,77,180,204,8,81,
    70,223,11,38,24,254,210,210,177,32,81,195,243,125,8,169,112,32,97,53,195,13,
    203,9,47,104,125,117,114,124,165,203,181,235,193,206,70,180,174,0,167,181,41,
    164,30,116,127,198,245,146,87,224,149,206,57,4,192,210,65,210,129,240,178,105,
    228,108,245,148,140,40,35,195,38,58,65,207,215,253,65,85,208,76,62,3,237,55,89,
    232,50,217,64,244,157,199,121,252,90,17,212,203,149,152,140,187,234,177,73,174,
    193,100,192,143,97,53,145,135,19,103,13,90,135,151,199,91,239,247,33,39,145,
    101,120,99,3,186,86,99,41,237,203,111,79,220,135,158,42,30,154,120,67,87,167,
    135,176,183,191,253,115,184,21,233,58,129,233,142,39,128,211,118,137,139,255,
    114,20,218,113,154,27,127,246,250,1,8,198,250,209,92,222,173,21,88,102,219
};

static int noise2(int x, int y){
    int  yindex = (y + SEED) % 256;
    if (yindex < 0)
        yindex += 256;
    int  xindex = (HASH[yindex] + x) % 256;
    if (xindex < 0)
        xindex += 256;
    const int  result = HASH[xindex];
    return result;
}

static double lin_inter(double x, double y, double s){
    return x + s * (y-x);
}

static double smooth_inter(double x, double y, double s){
    return lin_inter( x, y, s * s * (3-2*s) );
}

static double noise2d(double x, double y){
    const int  x_int = floor( x );
    const int  y_int = floor( y );
    const double  x_frac = x - x_int;
    const double  y_frac = y - y_int;
    const int  s = noise2( x_int, y_int );
    const int  t = noise2( x_int+1, y_int );
    const int  u = noise2( x_int, y_int+1 );
    const int  v = noise2( x_int+1, y_int+1 );
    const double  low = smooth_inter( s, t, x_frac );
    const double  high = smooth_inter( u, v, x_frac );
    const double  result = smooth_inter( low, high, y_frac );
    return result;
}

double perlin2d(double x, double y, double freq, int depth){
    double  xa = x*freq;
    double  ya = y*freq;
    double  amp = 1.0;
    double  fin = 0;
    double  div = 0.0;
    for (int i=0; i<depth; i++)
    {
        div += 256 * amp;
        fin += noise2d( xa, ya ) * amp;
        amp /= 2;
        xa *= 2;
        ya *= 2;
    }
    return fin/div;
}

#pragma endregion


void text(SDL_Point pos,SDL_Texture* font, const char* text,int scale) {
	int size = strlen(text);

	SDL_Rect glyph = { 0,0,5,8 };
	SDL_Rect render = { pos.x,pos.y,5 * scale,8 * scale };

	for (size_t x = 0; x < size; x++) {
		int ascii = text[x]-32;
		glyph.x = ((ascii % 16) * (glyph.w + 1));
		glyph.y = ((ascii / 16) * glyph.h);
		SDL_RenderCopy(renderer, font, &glyph, &render);
		render.x += 5 * scale;
	}
}

int clamp(int num,int min, int max){
    if(num < min) return min;
    if(num > max) return max;
    return num;
}

void key_callback(SDL_Event event){
    Uint8 old_ctrl = control;
    if(event.type == SDL_KEYDOWN){
        switch (event.key.keysym.sym){
        case SDLK_UP:   case SDLK_w:  control |= up;    break;
        case SDLK_DOWN: case SDLK_s:  control |= down;  break;
        case SDLK_LEFT: case SDLK_a:  control |= left;  break;
        case SDLK_RIGHT:case SDLK_d:  control |= right; break;
        case SDLK_m: Mix_VolumeMusic(volume-=(volume>0)?16:0); break;
        case SDLK_n: Mix_VolumeMusic(volume-=(volume>0)?16:0); break;
        }
    }else{
        switch (event.key.keysym.sym){
        case SDLK_UP:   case SDLK_w:  control &= ~up;    break;
        case SDLK_DOWN: case SDLK_s:  control &= ~down;  break;
        case SDLK_LEFT: case SDLK_a:  control &= ~left;  break;
        case SDLK_RIGHT:case SDLK_d:  control &= ~right; break;
        }
    }
    if(old_ctrl != control) key_change = true;


}

///game funcs



void start(){
    FPS = 60;
    SEED = time(NULL);
    srand(SEED);
    total_time = 0;
    
    root = (SDL_Point){worldw/2,0};
    timer = 60 * 60;

    for (size_t i = 0; i < 11; i++){
        textures[i].h = 16;
        textures[i].w = 16;
    }
    

    for (size_t x = 0; x < worldw; x++){
        for (size_t y = 0; y < worldh; y++){
            #define actual world[x][y]
            actual.content = nothing;

            if(y==0)actual.type = grass;
            else if(y < 6){
                actual.type = dirt;
                if(rand()%300==1)actual.content = water;
            }else if(y < 90){
                float perlin = perlin2d(x,y,0.3,1);
                if (perlin > 0.7){
                    actual.type = rock;
                }else{
                    actual.type = dirt;
                }
                if(rand()%250==1)actual.content = water;
                if(rand()%700==1)actual.content = coal;
            }else if(y < 250){
                float perlin = perlin2d(x,y,0.4,2);
                if (perlin > 0.55){
                    actual.type = rock;
                }else{
                    actual.type = dirt;
                }
                if(rand()%170==1)actual.content = water;
                if(rand()%400==1)actual.content = coal;
                if(rand()%1300==1 && actual.type == rock)actual.content = iron;
            }else{
                float perlin = perlin2d(x,y,0.4,2);
                if (perlin > 0.7){
                    actual.type = hard_rock;
                }else if(perlin > 0.4){
                    actual.type = rock;
                }else{
                    actual.type = dirt;
                }
                if(rand()%400==1)actual.content = water;
                if(rand()%300==1)actual.content = coal;
                if(rand()%1000==1)actual.content = iron;
                if(rand()%1200==1 && actual.type == hard_rock)actual.content = gold;
            }


            #undef actual
        }
    }


}

void input(){
    SDL_Event event;
    while (SDL_PollEvent(&event)){
        switch (event.type){
        case SDL_QUIT: running = false; break;
        case SDL_KEYDOWN: case SDL_KEYUP: key_callback(event);break;

        }
    }
}

// game loop stuff
void loop(){

    world[clamp(root.x,0,worldw)][clamp(root.y,0,worldh)].content = rooted;

    static Uint8 rooting_timer,tile_type,tile_content;
    static SDL_Point rooting_tile;
    static bool update,do_rooting;

    if(key_change){
        rooting_timer = 0;
        if(control != 0){
            update = true;
        }
    }

    if(rooting_timer == 5 && tile_content == rooted){
        do_rooting = true;
    }else if(rooting_timer == 20 && (tile_type == dirt || tile_type == grass)){
        do_rooting = true;
    }else if(rooting_timer == 50 && tile_type == rock){
        do_rooting = true;
    }else if(rooting_timer == 80 && tile_type == hard_rock){
        do_rooting = true;
    }

    if(do_rooting){
        root = (SDL_Point){clamp(rooting_tile.x +root.x,0,worldw-1),clamp(rooting_tile.y+root.y,0,worldh-1)};
        switch (world[root.x][root.y].content){
        case water:timer += 5 * 60; break;
        case coal:timer += 15 * 60; break;
        case iron:timer += 25 * 60; break;
        case gold:timer += 60 * 60; break;
        }
        update = true;
        do_rooting = false;
    }

    if(update){
        rooting_tile.x =   (control & left)?-1:(control & right)?1:0;
        rooting_tile.y =   (control & up)?-1: (control & down)?1:0;
        rooting_timer = 0;
        tile_type = world[root.x+rooting_tile.x][root.y+rooting_tile.y].type;
        tile_content = world[root.x+rooting_tile.x][root.y+rooting_tile.y].content;

        update = false;
    }
    if(control != 0){
        rooting_timer++;
    }

    camera.x += (((root.x*32)-(1280/2)-16)-camera.x)*0.20;
    camera.y += (((root.y*32)-(720/2)-16)-camera.y)*0.20;
    
    camera.y = clamp(camera.y,-(720/2),(worldh*32) - 720);
    camera.x = clamp(camera.x,0,(worldw*32) - 1280);

    timer--;
    total_time++;

    key_change = false;

    if(timer == 0){
        start();
    }

}

void render(){
    SDL_SetRenderDrawColor(renderer,46, 104, 163,255);
    SDL_RenderClear(renderer);

    SDL_Rect scrn = {0,0,1280,720};
    SDL_SetRenderDrawColor(renderer,120, 195, 227,255);
    SDL_RenderFillRect(renderer,&scrn);

    SDL_Rect rec = {0,0,32,32};

    for (int x = 0; x < worldw; x++){

        if((x*32)-camera.x >= 1280 || (x*32)-camera.x <= -32) continue;

        rec.x = (x * 32)-camera.x;
        for (int y = 0; y < worldh; y++){

            if((y*32)-camera.y >= 720 || (y*32)-camera.y <= -32) continue;

            rec.y = (y * 32)-camera.y;

            int sprite_index;
            switch (world[x][y].type){
            case dirt:sprite_index = 3; break;
            case grass:sprite_index = 2; break;
            case rock:sprite_index = 0; break;
            case hard_rock:sprite_index = 1; break;
            }
            SDL_RenderCopy(renderer,spritesheet,&textures[sprite_index],&rec);

            switch (world[x][y].content){
            case rooted:sprite_index = 9; break;
            case gold: sprite_index = 5; break;
            case iron: sprite_index = 6; break;
            case water: sprite_index = 7;break;
            case coal: sprite_index = 8;break;
            }
            SDL_RenderCopy(renderer,spritesheet,&textures[sprite_index],&rec);

            

        }
    }
    
    for (int x = (worldw/2)-1; x < (worldw/2)+2; x++){
        rec.x = (x * 32)-camera.x;
        for (int y = -15; y < 0; y++){
            rec.y = (y * 32)-camera.y;
            SDL_RenderCopy(renderer,spritesheet,&textures[4],&rec);
            
        }
    }
    

    rec.x = (root.x*32)-camera.x;
    rec.y = (root.y*32)-camera.y;

    SDL_RenderCopy(renderer,spritesheet,&textures[10],&rec);

    static char buffer[40];

    sprintf(buffer,"FPS: %d",real_FPS);
    text((SDL_Point){20,50},font,buffer,3);

    sprintf(buffer,"time left: %d m %d s",(timer/60)/60,(timer/60)%60);
    text((SDL_Point){20,80},font,buffer,3);

    sprintf(buffer,"total time: %d m %d s",(total_time/60)/60,(total_time/60)%60);
    text((SDL_Point){20,110},font,buffer,3);

    sprintf(buffer,"depth: %d",root.y);
    text((SDL_Point){20,140},font,buffer,3);

    SDL_RenderPresent(renderer);
}

void end(){

}

/// end of game funcs
#ifdef WINDOWS_BUILD
#define main WinMain
#endif
int main(int argc,char*argv[]){
    // load used SDL modules 
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_AUDIO);
    
    // load SDL image loader
    IMG_Init(IMG_INIT_PNG);

    // load SDL music loader/player
    Mix_Init(MIX_INIT_MP3);
    Mix_OpenAudio( 22050, MIX_DEFAULT_FORMAT, 2, 4096 );

    //create window and renderer
    SDL_Window* window = SDL_CreateWindow("SquareRoot mining",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,1280,720,SDL_WINDOW_RESIZABLE);

    renderer = SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if(!renderer)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Graphics Error",SDL_GetError(),window);

    SDL_RenderSetLogicalSize(renderer,1280,720);

    #ifdef LINUX_BUILD
    spritesheet = IMG_LoadTexture(renderer,"assets/tiles.png");
    font = IMG_LoadTexture(renderer,"assets/font.png");
    soundtrack = Mix_LoadMUS("assets/soundtrack.mp3");
    #endif 
    #ifdef WINDOWS_BUILD
    spritesheet = IMG_LoadTexture(renderer,"assets\\tiles.png");
    font = IMG_LoadTexture(renderer,"assets\\font.png");
    soundtrack = Mix_LoadMUS("assets\\soundtrack.mp3");
    #endif

    start();
    
    Mix_PlayMusic(soundtrack,-1);
    //game loop
    Uint32 frame_time = SDL_GetTicks(), interval = 1000/FPS,log_timer = SDL_GetTicks();
    Uint8 frame_count;
    running = true;
    
    while (running){
		frame_time = SDL_GetTicks();

        input();

        loop();

        render();
        
		frame_time = SDL_GetTicks() - frame_time;


		if (interval > frame_time) {
			SDL_Delay(interval - frame_time);
		}
        //SDL_Delay(interval);


        frame_count++;
        if (SDL_GetTicks() - log_timer > 1000) {
            real_FPS = frame_count;
			frame_count = 0;
            log_timer = SDL_GetTicks();
        }
    }

    end();
    
    //free objects
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    // unload SDL modules and extensions
    Mix_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}
