void VGA_text(int x, int y, char * text_ptr);
void VGA_text_clear();
void play_music();
float map(float value, float istart, float istop, float ostart, float ostop);

// global variables
volatile int pixel_buffer_start;
volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
int dx_box[12], dy_box[12];
int x_box[12] = {152, 191, 217, 230, 217, 191, 152, 113, 87, 74, 87, 113};
int y_box[12] = {30, 50, 76, 108, 140, 166, 186, 166, 140, 108, 76, 50};
short int color_box[4] = {0xD47F, 0x96FE, 0xF7F1, 0xAFD3}; //purple blue yellow green
int HEX_bits[10] = {0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01100111};
bool change = false;
int score = 0;
int box_num;
short int color = 0xD47F;
bool cw = true; 
int count = -1;
bool can_draw = false;
int buffer_index = 0;

//Declaration of 3 arrays - 1 audio, 2 images
//Warning: audio and image arrays removed to save file space for upload (consisted of 45,000 lines)

int music[] = {};

unsigned int bg[] = {};

int main(void)
{	
	//Interrupts - KEYs
	disable_A9_interrupts(); // disable interrupts in the A9 processor
	set_A9_IRQ_stack(); // initialize the stack pointer for IRQ mode
	config_GIC(); // configure the general interrupt controller
	config_KEYs(); // configure pushbutton KEYs to generate interrupts
	enable_A9_interrupts();
		
    //Declare variables
	srand(time(NULL));
	bool first = true;
	bool display_title = true;
	
	//Display title screen
	//PS2 Input
	volatile int * PS2_ptr = (int *)PS2_BASE;
	int PS2_data, RVALID;
	char byte1 = 0;
	*(PS2_ptr) = 0xFF; // reset
		
	pixel_buffer_start = *pixel_ctrl_ptr;
    clear_screen();
	
	VGA_text_clear();
	
	while(display_title) {
		//Audio - Background music, only when title screen displays
		play_music();
		
        //Draw background screen
		draw_bg(title);

		//initialize ps/2
		PS2_data = *(PS2_ptr); // read the Data register in the PS/2 port
		RVALID = PS2_data & 0x8000; // extract the RVALID field
		
		if (RVALID) {
			byte1 = PS2_data & 0xFF;

			if (byte1 == (char)0x5A) {
				//stops displaying title screen to start game
				display_title = false;	

				if (byte1 == (char)0x00) {
					*(PS2_ptr) = 0xF4;
				}
			}
		}
	}

    /* set front pixel buffer to start of FPGA On-chip memory */
    *(pixel_ctrl_ptr + 1) = 0xC8000000; // first store the address in the 
                                        // back buffer
    /* now, swap the front/back buffers, to set the front buffer location */
    wait_for_vsync();
    /* initialize a pointer to the pixel buffer, used by drawing functions */
    pixel_buffer_start = *pixel_ctrl_ptr;
    clear_screen(); // pixel_buffer_start points to the pixel buffer
    /* set back pixel buffer to start of SDRAM memory */
    *(pixel_ctrl_ptr + 1) = 0xC0000000;
    pixel_buffer_start = *(pixel_ctrl_ptr + 1); // we draw on the back buffer

	//A9 Private Timer - create delays and speed up the game
	
	volatile int * MPcore_private_timer_ptr = (int *)MPCORE_PRIV_TIMER;	
	int counter; // delay = 1/(200 MHz) x 100x10^6 = 0.5 sec	
	
    while (1)
    {	
		//Control the speed of the moving square
		//Initialize counter value used in interrupts
		if (first) {
			counter = 1;
			first = false;
		}
		else if (score < 3) {
			counter = 100000000;
		}
		else if (score >= 3 && score < 6) {
			counter = 70000000;
		}
		else if (score >= 6 && score < 9) {
			counter = 40000000;
		}
		else if (score >= 9 && score < 12) {
			counter = 2000000;
		}
		else if (score >= 12) {
			counter = 1;
		}
		
		*(MPcore_private_timer_ptr) = counter; // write to timer load register
		*(MPcore_private_timer_ptr + 2) = 0b011; // mode = 1 (auto), enable = 1
		
		while (*(MPcore_private_timer_ptr + 3) == 0)
		; // wait for timer to expire
		*(MPcore_private_timer_ptr + 3) = 1; // reset timer flag bit
		
        // code for drawing the boxes and lines and redrawing
		draw();
		
        wait_for_vsync(); // swap front and back buffers on VGA vertical sync
        pixel_buffer_start = *(pixel_ctrl_ptr + 1); // new back buffer
		
    }
}

//maps values from -32767-32767 to 0-2000000000
float map(float value, float istart, float istop, float ostart, float ostop) {
	return ostart + (ostop - ostart) * ((value - istart) / (istop - istart));
}

void play_music() {
	//Background music
	volatile int* audio_ptr = (int*)AUDIO_BASE;
	int fifospace;
	
	fifospace = *(audio_ptr + 1); // read the audio port fifospace register
			if ((fifospace & 0x00FF0000) > BUF_THRESHOLD) // check WSRC
			{
				// output data until the buffer is empty or the audio-out FIFO
				// is full
				float x = (900 * 3.14159 * 2) / 40000;
				float acc = 0;

				while ((fifospace & 0x00FF0000) && (buffer_index < BUF_SIZE)) {
					acc += x;
					*(audio_ptr + 2) = (int)map(music[buffer_index], -32767, 32767, 0, 2000000000);//(int)(sin(acc) * 10000000000);
					*(audio_ptr + 3) = (int)map(music[buffer_index], -32767, 32767, 0, 2000000000);
					++buffer_index;
					if (buffer_index == BUF_SIZE) {
						// done playback, music ends (lasts about 5 secs or else array size too large)
					}
					fifospace = *(audio_ptr + 1); // read the audio port fifospace register
				}
			}
}

void draw() {
	// Erase any boxes and lines that were drawn in the last iteration
	clear_screen();
	
	//Draw background image (arcade game board)
	draw_bg(bg);
	
	//Display score and instructions to character buffer.
	display_score();

	//flip direction of moving square after each 'hit'
	if (cw == true) {
		if(count == 11){
			count = 0;
		} else {
			count++;
		}
	} else {
		if (count == 0) {
			count = 11;
		} else {
			count--;
		}
	}
	
	//change square to random colour after each 'hit'
	if (change == true) {
		short int new_color = color_box[rand()% 4];
		
		while (color == new_color){
			new_color = color_box[rand()% 4];
		}
		
		color = new_color;
		change = false;
	}
	
	//draw 4 pointers on the screen
	draw_pointerh(39, 109, color_box[0]);	//purple
	draw_pointerh(261, 110, color_box[1]); 	//blue
	draw_pointerv(157, 2, color_box[2]);    //yellow
	draw_pointerv(157, 211, color_box[3]); 	//green
	
	//code for drawing the 12 boxes in a ring shape
	for (int i = 0; i < 12; ++i) {
		
		if (i == count) {
			box_num = i;
			//only one box is coloured at a time
			draw_box(i, color);
		}
		else {
		draw_box(i, 0xFFFF);
		}
	}

}

void draw_box(int box, short int color) {
	int width = 0;
	int height = 0;
	
	for (int i = 0; i < 82; ++i) {
		if (width == 9){
			width = 0;
			height += 1;
		}
		if (height == 9) {
			return;
		}
		plot_pixel(x_box[box] + width, y_box[box] + height, color);
		width += 1;
	}
	
}	

void draw_pointerh(int x, int y, short int color) {
	int width = 0;
	int height = 0;
	
	for (int i = 0; i < 37; ++i) {
		if (width == 12){
			width = 0;
			height += 1;
		}
		if (height == 3) {
			return;
		}
		plot_pixel(x + width, y + height, color);
		width += 1;
	}	
}	

void draw_pointerv(int x, int y, short int color) {
	int width = 0;
	int height = 0;
	
	for (int i = 0; i < 37; ++i) {
		if (width == 3){
			width = 0;
			height += 1;
		}
		if (height == 12) {
			return;
		}
		plot_pixel(x + width, y + height, color);
		width += 1;
	}	
}	

void draw_bg(unsigned int bg[]) {
	 int i = 0, j = 0;

    for (int k = 0; k < 320 * 2 * 240 - 1; k += 2) {

        int red = ((bg[k + 1] & 0xF8) >> 3) << 11;
        int green = (((bg[k] & 0xE0) >> 5)) | ((bg[k + 1] & 0x7) << 3);

        int blue = (bg[k] & 0x1f);

        short int p = red | ((green << 5) | blue);

        plot_pixel(0 + i, j, p);

        i += 1;
        if (i == 320) {
            i = 0;
            j += 1;
        }
    }	
}

void swap(int *x, int *y) { 
    int temp = *x; 
    *x = *y; 
    *y = temp; 
} 

void wait_for_vsync() {
	register int status;
	*pixel_ctrl_ptr = 1;  //start sync process
	status = *(pixel_ctrl_ptr + 3); //points to address 0xFF20302C
	
	while ((status & 0x01) != 0) { //wait for S to become 0, once S is 0, swap happens
		status = *(pixel_ctrl_ptr + 3);
	}
}

void draw_line(int x0, int y0, int x1, int y1, short int color) {
	bool is_steep = abs(y1 - y0) > abs(x1 - x0);
	if (is_steep) {
		swap(&x0, &y0);
		swap(&x1, &y1);
	}
	if (x0 > x1) {
		swap(&x0, &x1);
		swap(&y0, &y1);
	}
		
	int deltax = x1 - x0;
	int deltay = abs(y1- y0);
	int error = -(deltax / 2);
	int y = y0;
	int y_step;
	
	if (y0 < y1) {
		y_step = 1;
	}
	else {
		y_step = -1;
	}
	
	for (int x = x0; x <= x1; ++x) {
		if (is_steep) {
			plot_pixel(y, x, color);
		}
		else {
			plot_pixel(x, y, color);
		}
		error = error + deltay;
		
		if (error >= 0) {
			y = y + y_step;
			error = error - deltax;	
		}
	}
}

void clear_screen() {
	for (int x = 0; x < 320; ++x) {
		for (int y = 0; y < 240; ++y) {
			plot_pixel(x, y, 0x0000);
		}
	}
}

void plot_pixel(int x, int y, short int line_color) {
    *(short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = line_color;
}

//Character Buffer

/* write a single character to the character buffer at x,y
 * x in [0,79], y in [0,59]
 */
void write_char(int x, int y, char c) {
  // VGA character buffer
  volatile char * character_buffer = (char *) (0xC9000000 + (y<<7) + x);
  *character_buffer = c;
}

void display_score() {
	VGA_text_clear();

	char* instr = "HOW TO PLAY: Press KEY0 when square reaches the correct colour";
	int x = 10;
	while(*instr) {
		write_char(x, 58, *instr);
		x++;
		instr++;
	}
	
	char* sc = "SCORE:";
	VGA_text(4, 4, sc);
	
	char scor[5];
	
	itoa(score, scor, 10);
	VGA_text(11, 4, scor);

}

void VGA_text(int x, int y, char * text_ptr)
{
  	volatile char * character_buffer = (char *) 0xC9000000;	// VGA character buffer
	int offset;
	/* assume that the text string fits on one line */
	offset = (y << 7) + x;
	while ( *(text_ptr) )
	{
		// write to the character buffer
		*(character_buffer + offset) = *(text_ptr);	
		++text_ptr;
		++offset;
	}
}

void VGA_text_clear()
{
  	volatile char * character_buffer = (char *) 0xC9000000;	
	int offset, x, y;
	for (x=0; x<79; x++){
		for (y=0; y<59; y++){
			offset = (y << 7) + x;
			*(character_buffer + offset) = ' ';		
		}
	}
}

//Interrupt for KEYs

void pushbutton_ISR(void) {
	/* KEY base address */
	volatile int * KEY_ptr = (int *) 0xFF200050;
	/* HEX display base address */
	volatile int * HEX3_HEX0_ptr = (int *) 0xFF200020;
	int press, display = HEX_bits[score], tens, ones;
	press = *(KEY_ptr + 3); // read the pushbutton interrupt register
	*(KEY_ptr + 3) = press; // Clear the interrupt
	
	//check if any key button has been pressed
	if (press & 0x1) {// KEY0
		bool accurate = false;
	
		//compare the location of the box with the pointer location
		if (color == color_box[0]) {
			//check to see if box drawn is at correct x box, y box
			if ((box_num == 9) | (box_num == 10)) //made the target pointer larger so game is easier to play on cpulator
				accurate = true;
		}
		else if (color == color_box[1]) {
			if ((box_num == 3) | (box_num == 4))
				accurate = true;
		}
		else if (color == color_box[2]) {
			if ((box_num == 0) | (box_num == 1))
				accurate = true;
		}
		else if (color == color_box[3]) {
			if ((box_num == 6) | (box_num == 7))
				accurate = true;
		}	
		
		//change box color, increase score and change direction
		if (accurate) {
			//change color of box
			change = true;
			accurate = false;
			
			if (cw == true) {
				cw = false;
			} else {
				cw = true;
			}
			
			score++;
			tens = score;
			//divide function for 2 digit scores
			if (score > 9) {
				ones = tens % 10; //assumes highest score possible is 99
				tens = tens / 10;
				display = HEX_bits[tens] << 8 | HEX_bits[ones];
			}
			else {
				display = HEX_bits[score];
			}
		}
		else {
			//reset score
			score = 0;
			display = HEX_bits[score];
		}
		
	}
	
	//display_score on hex
	*HEX3_HEX0_ptr = display;
	
	return;
}

void config_KEYs() {
	volatile int * KEY_ptr = (int *) 0xFF200050; // pushbutton KEY base address
	*(KEY_ptr + 2) = 0xF; // enable interrupts for the two KEYs
}

void __attribute__((interrupt)) __cs3_isr_irq(void) {
	// Read the ICCIAR from the CPU Interface in the GIC
	int interrupt_ID = *((int *)0xFFFEC10C);
	if (interrupt_ID == 73) // check if interrupt is from the KEYs
		pushbutton_ISR();
	//else if (interrupt_ID == 72) // check if interrupt is from the Altera timer
		//interval_timer_ISR();
	else
		while (1); // if unexpected, then stay here
	// Write to the End of Interrupt Register (ICCEOIR)
	*((int *)0xFFFEC110) = interrupt_ID;
}

// Define the remaining exception handlers
void __attribute__((interrupt)) __cs3_reset(void) {
	while (1);
}
void __attribute__((interrupt)) __cs3_isr_undef(void) {
	while (1);
}
void __attribute__((interrupt)) __cs3_isr_swi(void) {
	while (1);
}
void __attribute__((interrupt)) __cs3_isr_pabort(void) {
	while (1);
}
void __attribute__((interrupt)) __cs3_isr_dabort(void) {
	while (1);
}
void __attribute__((interrupt)) __cs3_isr_fiq(void) {
	while (1);
}

/*
* Turn off interrupts in the ARM processor
*/
void disable_A9_interrupts(void) {
	int status = 0b11010011;
	asm("msr cpsr, %[ps]" : : [ps] "r"(status));
}

/*
* Initialize the banked stack pointer register for IRQ mode
*/
void set_A9_IRQ_stack(void) {
	int stack, mode;
	stack = 0xFFFFFFFF - 7; // top of A9 onchip memory, aligned to 8 bytes
	/* change processor to IRQ mode with interrupts disabled */
	mode = 0b11010010;
	asm("msr cpsr, %[ps]" : : [ps] "r"(mode));
	/* set banked stack pointer */
	asm("mov sp, %[ps]" : : [ps] "r"(stack));
	/* go back to SVC mode before executing subroutine return! */
	mode = 0b11010011;
	asm("msr cpsr, %[ps]" : : [ps] "r"(mode));
}

/*
* Turn on interrupts in the ARM processor
*/
void enable_A9_interrupts(void) {
	int status = 0b01010011;
	asm("msr cpsr, %[ps]" : : [ps] "r"(status));
}

/*
* Configure the Generic Interrupt Controller (GIC)
*/
void config_GIC(void) {
	config_interrupt (73, 1); // configure the FPGA KEYs interrupt (73)
	// Set Interrupt Priority Mask Register (ICCPMR). Enable interrupts of all
	// priorities
	*((int *) 0xFFFEC104) = 0xFFFF;
	// Set CPU Interface Control Register (ICCICR). Enable signaling of
	// interrupts
	*((int *) 0xFFFEC100) = 1;
	// Configure the Distributor Control Register (ICDDCR) to send pending
	// interrupts to CPUs
	*((int *) 0xFFFED000) = 1;
	
	/* configure the FPGA interval timer and KEYs interrupts */
	*((int *)0xFFFED848) = 0x00000101;
}

/*
* Configure Set Enable Registers (ICDISERn) and Interrupt Processor Target
* Registers (ICDIPTRn). The default (reset) values are used for other registers
* in the GIC.
*/
void config_interrupt(int N, int CPU_target) {
	int reg_offset, index, value, address;
	/* Configure the Interrupt Set-Enable Registers (ICDISERn).
	* reg_offset = (integer_div(N / 32) * 4
	* value = 1 << (N mod 32) */
	reg_offset = (N >> 3) & 0xFFFFFFFC;
	index = N & 0x1F;
	value = 0x1 << index;
	address = 0xFFFED100 + reg_offset;
	/* Now that we know the register address and value, set the appropriate bit */
	*(int *)address |= value;
	/* Configure the Interrupt Processor Targets Register (ICDIPTRn)
	* reg_offset = integer_div(N / 4) * 4
	* index = N mod 4 */
	reg_offset = (N & 0xFFFFFFFC);
	index = N & 0x3;
	address = 0xFFFED800 + reg_offset + index;
	/* Now that we know the register address and value, write to (only) the
	* appropriate byte */
	*(char *)address = (char)CPU_target;
}
