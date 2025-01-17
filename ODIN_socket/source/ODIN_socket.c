#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>

#include <fcntl.h>

#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <3ds.h>

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000

#define PIXELS_AMOUNT 96000
#define BYTES_IN_GFX_BUFFER 288000
#define BYTES_IN_IMAGE 192000
#define BYTES_PER_BATCH 512
#define PIXELS_PER_BATCH 256
#define TOP_WIDTH 400
#define TOP_HEIGHT 240

static u32* SOC_buffer = NULL;

PrintConsole topScreen, bottomScreen;

__attribute__((format(printf, 1, 2)))
void failExit(const char* fmt, ...);

void print_bottom(char* fmt, ...){
	va_list argptr;
    va_start(argptr, fmt);

	consoleSelect(&bottomScreen);
    vprintf(fmt, argptr);
    
	va_end(argptr);
}

// void print_top(char* fmt, ...){
// 	va_list args;
// 	//va_start(args,n);

// 	//consoleSelect(&topScreen);
// 	//printf(fmt,args);

// 	//va_end(args);
// }

void socShutdown() {
	print_bottom("waiting for socExit...\n");
	socExit();

}

void init(){
	// Init main parameters

	int ret;

	print_bottom("\nPerforming init\n");
	gfxInitDefault();
	
	atexit(gfxExit);

	//consoleInit(GFX_TOP, &topScreen);
	consoleInit(GFX_BOTTOM, &bottomScreen);

	// allocate buffer for SOC service
	SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
	if (SOC_buffer == NULL) {
		failExit("memalign: failed to allocate\n");
	}
	
	// Now intialise soc:u service
	if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
		failExit("socInit: 0x%08X\n", (unsigned int)ret);
	}

	// register socShutdown to run at exit
	// atexit functions execute in reverse order so this runs before gfxExit
	atexit(socShutdown);

	gfxSetDoubleBuffering(GFX_TOP,true);
	gfxSetWide(false);

	print_bottom("\nInit complet\n");
}


unsigned long long pixel_position = 0;
void print_buffer(u8* frame_buffer, u16* pixels){
	// writes pixels to the screen
	// the amount of pixels specified in PIXELS_PER_BATCH

	if(pixel_position == 288000){
		gfxFlushBuffers();
		gfxScreenSwapBuffers(GFX_TOP,false);
		pixel_position = 0;
	}

	// end of the array
	u16* pixels_end = pixels + PIXELS_PER_BATCH;

	// iterate over pixels
	for(pixels; pixels<pixels_end; pixels += 1){
		u16 pixel = *pixels;

		uint8_t r = (pixel >> 11)<<3;
		uint8_t g = ((pixel >> 5) & 0x3F)<<2;
		uint8_t b = (pixel & 0x1F)<<3;

		frame_buffer[pixel_position] = r;
		frame_buffer[pixel_position+1] = g;
		frame_buffer[pixel_position+2] = b;

		pixel_position += 3;
	}


}

void print_whole_buffer(u8* frame_buffer, u16* pixels){
	// writes pixels to the screen
	// the amount of pixels specified in PIXELS_PER_BATCH

	unsigned long long pixel_position = 0;

	// end of the array
	u16* pixels_end = pixels + PIXELS_AMOUNT;

	// iterate over pixels
	for(pixels; pixels<pixels_end; pixels += 1){
		u16 pixel = *pixels;

		uint8_t r = (pixel >> 11)<<3;
		uint8_t g = ((pixel >> 5) & 0x3F)<<2;
		uint8_t b = (pixel & 0x1F)<<3;

		frame_buffer[pixel_position] = r;
		frame_buffer[pixel_position+1] = g;
		frame_buffer[pixel_position+2] = b;

		pixel_position += 3;
	}

	gfxFlushBuffers();
	gfxScreenSwapBuffers(GFX_TOP,false);

}


s32 server_sock = -1, client_sock = -1;

int main(int argc, char** argv) {
	int ret;

	bool run_main_loop = true;

	bool stop_circle = true;

	struct sockaddr_in client;
	struct sockaddr_in server;
	
	u8* recv_buffer = malloc(BYTES_IN_IMAGE);
	u32 last_recieved_size = 0;

	u32	clientlen = sizeof(client);

	// main init
	//memset(recv_buffer, 0, BYTES_IN_GFX_BUFFER);
	memset(&server, 0, sizeof(server));
	memset(&client, 0, sizeof(client));
	init();

	//u8* frame_buffer;
	u8* frame_buffer = gfxGetFramebuffer(GFX_TOP,
										 GFX_LEFT,
										 NULL, 
										 NULL);
	memset(frame_buffer, 0, 240*400*3);

	server.sin_family = AF_INET;
	server.sin_port = htons(80);
	server.sin_addr.s_addr = gethostid();

	// create server socket
	server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (server_sock < 0) {
		failExit("socket: %d %s\n", errno, strerror(errno));
	}

	print_bottom("Connect to %s\n", inet_ntoa(server.sin_addr));

	// bind server socket
	if ((ret = bind(server_sock, (struct sockaddr*)&server, sizeof(server)))) {
		close(server_sock);
		failExit("bind: %d %s\n", errno, strerror(errno));
	}

	// Set  server socket non blocking 
	fcntl(server_sock, F_SETFL, fcntl(server_sock, F_GETFL, 0) | O_NONBLOCK);

	if ((ret = listen(server_sock, 1))) {
		failExit("listen: %d %s\n", errno, strerror(errno));
	}

	// start main app loop
	while (aptMainLoop() && run_main_loop) {
		hidScanInput();

		// read pressed key
		u32 pressed_key = hidKeysDown();

		// accept connection
		client_sock = accept(server_sock, (struct sockaddr*)&client, &clientlen);
		
		
		if (pressed_key & KEY_START){
			break;
		}

		if (client_sock < 0) {
			if (errno != EAGAIN) {
				failExit("accept: %d %s\n", errno, strerror(errno));
			}
		}
		else {

			// set client socket to blocking to simplify sending data back
			print_bottom("Connecting port %d from %s\n", client.sin_port, inet_ntoa(client.sin_addr));
			
			while (client_sock != -1 && run_main_loop) {
				
				// recieving image
				u32 result = recv(client_sock, 
									recv_buffer + last_recieved_size,
									BYTES_IN_IMAGE - last_recieved_size, 0);
				if(result != -1){
					//print_bottom("Recieved:%d\n",result);
					last_recieved_size += result;
				}
				if (last_recieved_size == BYTES_IN_IMAGE) {
					print_bottom("Recieved:%d\n",last_recieved_size);
					//gspWaitForVBlank();
					// recieved data
					last_recieved_size = 0;


					frame_buffer = gfxGetFramebuffer(GFX_TOP,
													GFX_LEFT,
													NULL, 
													NULL);


					print_whole_buffer((u8*)frame_buffer,
										(u16*)recv_buffer);


					//memset(recv_buffer, 0, BYTES_PER_BATCH);

				}

				// scan and process pressed key
				hidScanInput();

				// read circle position
				circlePosition circle_position;
				hidCircleRead(&circle_position);

				switch(hidKeysDown()){
					case KEY_START:
						run_main_loop = false; 
						break;
					case KEY_A: 
						send(client_sock, "A,", 2, 0);
						//print_bottom("You pressed A\n");
						break;
					case KEY_B:
						send(client_sock, "B,", 2, 0);
						//print_bottom("You pressed B\n");
						break;
					case KEY_X:
						send(client_sock, "X,", 2, 0);
						//print_bottom("You pressed X\n");
						break;
					case KEY_Y: 
						send(client_sock, "Y,", 2, 0);
						//print_bottom("You pressed Y\n");
						break;
					case KEY_SELECT:
						send(client_sock, "SELECT,", 7, 0);
						//print_bottom("You pressed SELECT\n");
						break;
					case KEY_DUP:
						send(client_sock, "u,", 2, 0);
						//print_bottom("You pressed dUP\n");
						break;
					case KEY_DDOWN: 
						send(client_sock, "d,", 2, 0);
						//print_bottom("You pressed dDOWN\n");
						break;
				}

				if (((circle_position.dy > 25) || (circle_position.dy < -25)) || ((circle_position.dx > 25) || (circle_position.dx < -25))) {
					char posit[20];
					stop_circle = false;

					sprintf(posit, "%d;%d,", circle_position.dx, circle_position.dy);
					send(client_sock, posit, strlen(posit), 0);
					//print_bottom("Circle x y pos is %d :  %d\n", circle_position.dx, circle_position.dy);
				}else if(!stop_circle){
					//circle is in the middle
					send(client_sock, "0", 1, 0);

					stop_circle = true;
						
					//print_bottom("we are stopped\n");
				}
				gfxScreenSwapBuffers(GFX_BOTTOM,false);
			}


		}



	}

	client_sock = -1;
	close(server_sock);
	gfxExit();
	return 0;
}

//---------------------------------------------------------------------------------
void failExit(const char* fmt, ...) {
	//---------------------------------------------------------------------------------

	if (server_sock > 0) close(server_sock);
	if (client_sock > 0) close(client_sock);

	va_list ap;

	printf(CONSOLE_RED);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf(CONSOLE_RESET);
	printf("\nPress B to exit\n");

	while (aptMainLoop()) {
		gspWaitForVBlank();
		hidScanInput();

		u32 kDown = hidKeysDown();
		if (kDown & KEY_B) exit(0);
	}
}
