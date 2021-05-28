#pragma region preprocessing
#define LINUX_BUILD
#ifdef LINUX_BUILD
    #include<SDL2/SDL.h>
    #include<SDL2/SDL_image.h>
    #include<SDL2/SDL_mixer.h>
    #define asset_path "assets/"
#endif
#ifdef WINDOWS_BUILD
    #include<SDL2/SDL.h>
    #include<SDL2/SDL_image.h>
    #include<SDL2/SDL_mixer.h>
    #define asset_path "assets\\"
    #define main WinMain
#endif
#ifdef __EMSCRIPTEN__

#endif

#include<stdio.h>
#include<math.h>
#include<time.h>
#include<stdbool.h>

#ifdef EMBED_ASSETS
#include"tiles.h"
#include"font.h"
#include"soundtrack.h"
#endif


#define chance(c)rand()%c==1

#pragma endregion

#pragma region data

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
    grass,
    dirt,
}TileType;

typedef enum{
    nothing,
    gold,
    iron,
    water,
    coal,
    rooted,
}ContentType;

typedef struct{
    Uint8 type;
    Uint8 content;
}Tile;

typedef struct{
    Uint8 break_time;
    Uint8 sprite;
}TileTypeData;

typedef struct{
    Uint8 time_value;
    Uint8 sprite;
}ContentTypeData;

//global variables

const Uint32 worldw = 1000;
const Uint32 worldh = 1000;
const int tileS = 32;

bool running,need_render;

Tile **world;

Uint8 FPS_log,control;
Uint32 timer, total_time;
Sint32 SEED;

SDL_Point camera,root;

//                      rock - hard rock - grass - dirt -       log    -   gold -   iron - water -    coal -        root    -  select
SDL_Rect textures[11] = {{0,0},{1*16,0}, {0,1*16},{1*16,1*16},{0,2*16},  {2*16,0},{3*16,0},{2*16,1*16},{3*16,1*16},{2*16,2*16},{1*16,2*16}};

TileTypeData tile_data[5] =         { {0,0},{50,0},{80,1},{20,2},{20,3} };
ContentTypeData content_data[6] =   { {0,0},{60,5},{25,6},{5,7},{15,8},{0,9}};

SDL_Renderer*   renderer;
SDL_Texture*    spritesheet,*font;
SDL_Surface     icon;//unused

Uint8 volume = 64;
Mix_Music*   soundtrack;


#pragma endregion data

//credits for perlin noise: https://gist.github.com/nowl/828013
#pragma region perlin noise
static int HASH[] = {208,34,231,213,32,248,233,56,161,78,24,140,71,48,140,254,245,255,247,247,40,
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
                     114,20,218,113,154,27,127,246,250,1,8,198,250,209,92,222,173,21,88,102,219};
static int noise2(int x, int y){
    int  yindex = (y + SEED) % 256;
    if (yindex < 0) yindex += 256;
    int  xindex = (HASH[yindex] + x) % 256;
    if (xindex < 0) xindex += 256;
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
    for (int i=0; i<depth; i++){
        div += 256 * amp;
        fin += noise2d( xa, ya ) * amp;
        amp /= 2;
        xa *= 2;
        ya *= 2;
    }
    return fin/div;
}
#pragma endregion perlin noise

#pragma region utility functions
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
    return(num < min)?min:(num > max)?max:num;
}

#pragma endregion utility functions

#pragma region game functions

void start(){
    SEED = time(NULL);
    srand(SEED);

    world = (Tile**)calloc(worldw,sizeof(Tile*));
    for (size_t i = 0; i < worldh; i++){
        world[i] = (Tile*)calloc(worldh,sizeof(Tile));
    }
    root = (SDL_Point){worldw/2,0};
    timer = 60 * 60;
    total_time = 0;

    for (size_t x = 0; x < worldw; x++){
        for (size_t y = 0; y < worldh; y++){
            Tile tile = {air,nothing};
            float perlin;

            int layer = (y==0)?0:(y<6)?1:(y<90)?2:(y<250)?3:4;
            
            switch (layer){
            case 0: tile.type = grass;break;
            case 1: tile.type = dirt; if(chance(300))tile.content = water; break;
            
            case 2: tile.type = (perlin2d(x,y,0.3,1)>0.7)?rock:dirt;
                    tile.content = (chance(700))?coal   :(chance(250))?water    :nothing;
                    break;
            case 3: tile.type = (perlin2d(x,y,0.4,2)>0.55)?rock:dirt;
                    tile.content = (chance(1300)&&tile.type==rock)?iron  :(chance(400))?coal     :(chance(170))?water    :nothing;
                    break;
            case 4: perlin = perlin2d(x,y,0.4,2);
                    tile.type = (perlin>0.7)?hard_rock  :(perlin>0.4)?rock      :dirt;
                    tile.content = (chance(1200)&&tile.type==hard_rock)?gold:(chance(1000))?iron  :(chance(300))?coal     :(chance(400))?water    :nothing;
                    break;
            }
            world[x][y] = tile;
        }
    }
}

void input(){
    SDL_Event event;
    while (SDL_PollEvent(&event)){
        switch (event.type){
        case SDL_QUIT: running = false; break;

        case SDL_KEYDOWN:       switch(event.key.keysym.sym){
            case SDLK_UP:   case SDLK_w:  control |= up;    break;
            case SDLK_DOWN: case SDLK_s:  control |= down;  break;
            case SDLK_LEFT: case SDLK_a:  control |= left;  break;
            case SDLK_RIGHT:case SDLK_d:  control |= right; break;
            case SDLK_m: Mix_VolumeMusic(volume+=(volume<128)?16:0); break;
            case SDLK_n: Mix_VolumeMusic(volume-=(volume>0)?16:0); break;
            }break;
        case SDL_KEYUP:         switch (event.key.keysym.sym){
            case SDLK_UP:   case SDLK_w:  control &= ~up;    break;
            case SDLK_DOWN: case SDLK_s:  control &= ~down;  break;
            case SDLK_LEFT: case SDLK_a:  control &= ~left;  break;
            case SDLK_RIGHT:case SDLK_d:  control &= ~right; break;
            }break;
        }
    }
}

// game update stuff
void update(){
    world[root.x][root.y].content = rooted;

    static Uint8 rooting_timer,tile_type,tile_content;
    static SDL_Point rooting_tile = {0,0};

    //check control change
    static Uint8 control_checker = 0;
    if(control_checker != control){
        control_checker = control;
        rooting_timer = 0;   
        if(control != 0){
            rooting_tile.x =  ((control & left) ?-1: (control & right)?1:0);
            rooting_tile.y =  ((control & up)?-1: (control & down) ?1:0);

            tile_type    =world[rooting_tile.x+root.x][rooting_tile.y+root.y].type;
            tile_content =world[rooting_tile.x+root.x][rooting_tile.y+root.y].content;
        }
    }
    // already broke the block?
    if((rooting_timer == 5 && tile_content == rooted) || rooting_timer == tile_data[tile_type].break_time  ){
        root = (SDL_Point){clamp(rooting_tile.x+root.x,0,worldw-1),clamp(rooting_tile.y+root.y,0,worldh-1)};
        
        timer += content_data[world[root.x][root.y].content].time_value * 60;

        rooting_tile.x =  ((control & left) ?-1: (control & right)?1:0);
        rooting_tile.y =  ((control & up)?-1: (control & down) ?1:0);
        rooting_timer = 0;

        tile_type    =world[rooting_tile.x+root.x][rooting_tile.y+root.y].type;
        tile_content =world[rooting_tile.x+root.x][rooting_tile.y+root.y].content;
    }
    // timing
    if(control != 0) rooting_timer++;
    
    //smooth cam
    camera.x += (((root.x*tileS)-(1280/2)-16)-camera.x)*0.20;
    camera.y += (((root.y*tileS)-(720/2)-16)-camera.y)*0.20;
    
    camera.x = clamp(camera.x,0,(worldw*tileS) - 1280);
    camera.y = clamp(camera.y,-(720/2),(worldh*tileS) - 720);

    total_time++;
    timer--;
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

    SDL_Rect rec = {0,0,tileS,tileS};

    for (int x = 0; x < worldw; x++){
        if((x*tileS)-camera.x >= 1280 || (x*tileS)-camera.x <= -tileS) continue;
        rec.x = (x * tileS)-camera.x;
        for (int y = 0; y < worldh; y++){
            if(world[x][y].type == air)continue;
            if((y*tileS)-camera.y >= 720 || (y*tileS)-camera.y <= -tileS) continue;

            rec.y = (y * tileS)-camera.y;
            SDL_RenderCopy(renderer,spritesheet,&textures[tile_data[world[x][y].type].sprite],&rec);

            if(world[x][y].content == nothing)continue;
            SDL_RenderCopy(renderer,spritesheet,&textures[content_data[world[x][y].content].sprite],&rec);
        }
    }
    // *toco*
    for (int x = (worldw/2)-1; x <= (worldw/2)+1; x++) for(int y = -12; y < 0; y++){
        rec = (SDL_Rect){(x*tileS)-camera.x,(y*tileS)-camera.y,tileS,tileS};
        SDL_RenderCopy(renderer,spritesheet,&textures[4],&rec);
    }
    
    rec.x = (root.x*tileS)-camera.x;
    rec.y = (root.y*tileS)-camera.y;
    SDL_RenderCopy(renderer,spritesheet,&textures[10],&rec);

    //UI
    static char buffer[40];

    sprintf(buffer,"FPS: %d",FPS_log);
    text((SDL_Point){20,50},font,buffer,3);
    sprintf(buffer,"time left: %d m %d s",(timer/60)/60,(timer/60)%60);
    text((SDL_Point){20,80},font,buffer,3);
    sprintf(buffer,"total time: %d m %d s",(total_time/60)/60,(total_time/60)%60);
    text((SDL_Point){20,110},font,buffer,3);
    sprintf(buffer,"depth: %d",root.y);
    text((SDL_Point){20,140},font,buffer,3);

    SDL_RenderPresent(renderer);
}
void loop(){
    input();
    update();
    render();
}
#pragma endregion

int main(int argc,char*argv[]){
    // load used SDL modules 
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_AUDIO);
    IMG_Init(IMG_INIT_PNG);
    Mix_Init(MIX_INIT_MP3);

    Mix_OpenAudio( 22050, MIX_DEFAULT_FORMAT, 2, 4096 );

    //create window and renderer
    SDL_Window* window  =SDL_CreateWindow("SquareRoot mining",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,1280,720,SDL_WINDOW_RESIZABLE);
    renderer            =SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    
    if(!renderer)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Graphics Error",SDL_GetError(),window);
    SDL_RenderSetLogicalSize(renderer,1280,720);
    
    //load assets
    #ifdef EMBED_ASSETS
    spritesheet         =IMG_LoadTexture_RW(renderer,SDL_RWFromConstMem(tiles_sprites,tiles_size),1);
    font                =IMG_LoadTexture_RW(renderer,SDL_RWFromConstMem(font_data,font_size),1);
    soundtrack          =Mix_LoadMUS_RW(SDL_RWFromConstMem(soundtrack_data,soundtrack_size),1);
    #else
    spritesheet         =IMG_LoadTexture(renderer,asset_path"tiles.png");
    font                =IMG_LoadTexture(renderer,asset_path"font.png");
    soundtrack          =Mix_LoadMUS(assetpath"soundtrack.mp3");
    #endif
    for (size_t i = 0; i < 11; i++){
        textures[i].h = 16;
        textures[i].w = 16;
    }
    start();
    
    Mix_PlayMusic(soundtrack,-1);
    
    SDL_Event event;

    //web browsers can't handle infinite loops
    #ifndef __EMSCRIPTEN__
    Uint8 frame_count,FPS = 60;
    Uint32 frame_time = SDL_GetTicks(), interval = 1000/FPS,log_timer = SDL_GetTicks();
    running = true;
    
    while (running){
		frame_time = SDL_GetTicks();
        
        loop();
        
		frame_time = SDL_GetTicks() - frame_time;

		if (interval > frame_time) SDL_Delay(interval - frame_time);
        
        frame_count++;
        if (SDL_GetTicks() - log_timer > 1000) {
            FPS_log = frame_count;
			frame_count = 0;
            log_timer = SDL_GetTicks();
        }
    }
    #else    

    #endif
    
    //free objects
    Mix_FreeMusic(soundtrack);
    SDL_DestroyTexture(spritesheet);
    SDL_DestroyTexture(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    // unload SDL modules and extensions
    Mix_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}