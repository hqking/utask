#include <stdio.h>
#include <process.h>
#include <windows.h>

void a(void *arg)
{
	int tmp = *(int *)arg;
	int i, j;

	for (i = 0; i < 5; i++) {
		Sleep(100);
		printf("from A is %d\n", tmp);
	}
	
}

int main(void)
{
	int data = 10;
	int i, j;

	_beginthread(a, 0, &data);

	for (i = 0; i < 5; i++) {
		printf("from main\n");
		Sleep(100);
	}
}
