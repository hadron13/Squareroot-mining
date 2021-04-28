# Squareroot mining
 ## my second Ludum Dare 48 entry for jam
 ## [Ludum dare submission](https://ldjam.com/events/ludum-dare/48/squareroot-mining)

# compiling
* ## before compiling: 
    1. ### create build-windows / linux directory and 'assets' directory inside
    2. ### copy game assets to it (or use transfer.sh)
* ## dependencies:
    * ### SDL2
    * ### SDL2_image
    * ### SDL2_mixer
* ## Windows (Mingw-w64)
    ` gcc source\\main.c -D WINDOWS_BUILD -o build-windows\\game.exe -lSDL2main -lSDL2 -lSDL2_image -lSDL2_mixer`
* ## Linux (gcc)
    `gcc source/main.c -D LINUX_BUILD -o build-linux\\game.program -lSDL2main -lSDL2 -lSDL2_image -lSDL2_mixer -lm`
