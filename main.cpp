#include "camera.h"

int main(void)
{
	Camera myCam;
		
	myCam.open("cctv.avi");
	
	myCam.play();
	
	myCam.close();
	
	return 1;
	
}
