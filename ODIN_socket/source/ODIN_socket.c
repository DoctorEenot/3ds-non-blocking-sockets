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

static u32* SOC_buffer = NULL;
s32 sock = -1, csock = -1;

__attribute__((format(printf, 1, 2)))
void failExit(const char* fmt, ...);


//---------------------------------------------------------------------------------
void socShutdown() {
	//---------------------------------------------------------------------------------
	printf("waiting for socExit...\n");
	socExit();

}

//---------------------------------------------------------------------------------
int main(int argc, char** argv) {
	//---------------------------------------------------------------------------------
	int ret;
	bool stop = true;

	u32	clientlen;
	struct sockaddr_in client;
	struct sockaddr_in server;
	char temp[1024];

	gfxInitDefault();

	// register gfxExit to be run when app quits
	// this can help simplify error handling
	atexit(gfxExit);

	PrintConsole topScreen, bottomScreen;
	consoleInit(GFX_TOP, &topScreen);
	consoleInit(GFX_BOTTOM, &bottomScreen);
	consoleSelect(&topScreen);

	printf("\nlibctru sockets demo\n");

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

	// libctru provides BSD sockets so most code from here is standard
	clientlen = sizeof(client);

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

	if (sock < 0) {
		failExit("socket: %d %s\n", errno, strerror(errno));
	}

	memset(&server, 0, sizeof(server));
	memset(&client, 0, sizeof(client));

	server.sin_family = AF_INET;
	server.sin_port = htons(80);
	server.sin_addr.s_addr = gethostid();

	printf("Connect to %s\n", inet_ntoa(server.sin_addr));

	if ((ret = bind(sock, (struct sockaddr*)&server, sizeof(server)))) {
		close(sock);
		failExit("bind: %d %s\n", errno, strerror(errno));
	}

	// Set socket non blocking so we can still read input to exit
	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);

	if ((ret = listen(sock, 5))) {
		failExit("listen: %d %s\n", errno, strerror(errno));
	}

	while (aptMainLoop()) {
		hidScanInput();
		u32 kDown = hidKeysDown();
		csock = accept(sock, (struct sockaddr*)&client, &clientlen);
		if (kDown & KEY_START) break;
		if (csock < 0) {
			if (errno != EAGAIN) {
				failExit("accept: %d %s\n", errno, strerror(errno));
			}
		}
		else {
			// set client socket to blocking to simplify sending data back
			//fcntl(csock, F_SETFL, fcntl(csock, F_GETFL, 0) & ~O_NONBLOCK);
			printf("Connecting port %d from %s\n", client.sin_port, inet_ntoa(client.sin_addr));
			while (csock != -1) {
				hidScanInput();
				circlePosition cpos;
				hidCircleRead(&cpos);
				consoleSelect(&topScreen);
				u32 kDown = hidKeysDown();
				memset(temp, 0, 1024);
				recv(csock, temp, 1024, 0);
				if (strcmp(temp, "") != 0) {
					consoleSelect(&bottomScreen);

					printf("%s\n", temp);
					consoleSelect(&topScreen);
				}
				if (kDown & KEY_START) break;
				if (kDown & KEY_A) {
					send(csock, "A,", 2, 0);
					printf("You pressed A\n");
				}
				if (kDown & KEY_B) {
					send(csock, "B,", 2, 0);
					printf("You pressed B\n");
				}
				if (kDown & KEY_X) {
					send(csock, "X,", 2, 0);
					printf("You pressed X\n");
				}
				if (kDown & KEY_Y) {
					send(csock, "Y,", 2, 0);
					printf("You pressed Y\n");
				}
				if (kDown & KEY_SELECT) {
					send(csock, "SELECT,", 7, 0);
					printf("You pressed SELECT\n");
				}
				if (kDown & KEY_DUP) {
					send(csock, "u,", 2, 0);
					printf("You pressed dUP\n");
				}
				if (kDown & KEY_DDOWN) {
					send(csock, "d,", 2, 0);
					printf("You pressed dDOWN\n");
				}

				if (((cpos.dy > 25) || (cpos.dy < -25)) || ((cpos.dx > 25) || (cpos.dx < -25))) {
					stop = false;
					char posit[20];
					sprintf(posit, "%d;%d,", cpos.dx, cpos.dy);
					send(csock, posit, strlen(posit), 0);
					printf("Circle x y pos is %d :  %d\n", cpos.dx, cpos.dy);
				}

				if (cpos.dy < 25 && cpos.dy > -25 && cpos.dx < 25 && cpos.dx > -25 && stop == false) {
					send(csock, "0", 1, 0);
					printf("we are stopped\n");
					stop = true;
				}
				gspWaitForVBlank();
				gfxFlushBuffers();
				gfxSwapBuffers();
			}


		}



	}

	csock = -1;
	close(sock);
	gfxExit();
	return 0;
}

//---------------------------------------------------------------------------------
void failExit(const char* fmt, ...) {
	//---------------------------------------------------------------------------------

	if (sock > 0) close(sock);
	if (csock > 0) close(csock);

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
