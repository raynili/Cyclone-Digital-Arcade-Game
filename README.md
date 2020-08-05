# Cyclone-Digital-Arcade-Game

Digital recreation of the classic arcade game. 
ECE243 Computer Organization Final Project
Team members: Rayni Li and Jenny Li
Technologies: Programmed in C and run on the DE1-SoC Board.

Note: Audio and background images have been removed to save file space. Previous took up 45,000 lines of code.

Setup: 
- Paste code into https://cpulator.01xz.net/?sys=arm-de1soc&d_audio=48000 to run.
- Move the seven-segment display, VGA pixel buffer and PS/2 keyboard (IRQ 79) together.
- Turn off device-specific warnings.
- The game starts with the title screen, there is some background music here.
- To start the game, press enter once on the PS/2 keyboard in the box ‘Type here’.

How to play the game:
- The aim of the game is to stop the cycling light when it reaches the pointer of the corresponding colour. 
- The player must press and release KEY0 when the coloured box reaches the pointer of the same colour. 
- If successful, the colour of the box will change and the player has a new colour to match with. The score will increase.
- If the key is pressed and released at the wrong location, the score will reset.

<img  width="400" alt="game_demo" src=images/game_demo.jpg/>
