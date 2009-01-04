#include "main.h"

App *App::mSingleton = NULL;
int returnCode = 0;

/*********************************************
		Application init
**********************************************/
int App::init(App *a, int argc, char **argv){

	mSingleton = a;

	//Banner
	LOG("\n");
	
	LOG("**********************************************\n");
	LOG("               BSOD Client\n");
	LOG("**********************************************\n");
		
	srand(time(0));
	bConnected = false;
	done = false;
	
	//Load the configuration
	if(!loadConfig()){
		LOG("Something went wrong with the config file, bailing out\n");
		return 0;
	}			
	
	//and SDL_net
	if(SDLNet_Init()==-1) {
	    LOG("SDLNet_Init: %s\n", SDLNet_GetError());
	    return 2;
	}
	
	//Connect
	if(mServerAddr != ""){
		if(!openSocket()){
			LOG("Something went wrong with the socket, bailing out\n");
			return 0;
		}
	}else{
		LOG("WARNING: No server specified - will generate phoney test data!\n");
	}
					 	
	//Start SDL
	if(SDL_Init(0)==-1) {
	    LOG("SDL_Init: %s\n", SDL_GetError());
	    return 1;
	}
	
			
	//Make the window	
	if(!utilCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP, SCREEN_FULLSCREEN)){
		ERR("Couldn't make window!\n");
		utilShutdown(1);
	}

	//Input
	keyInit();
		
	//Texturing
	texInit();
	
	//font
	initFont();
		
	//Time etc
	iFPS = 0;
	fTimeScale = iFPS / 1000;	
	fZoom = 0.0f;
	iTime = 0;
	
	//Particle system
	initParticleSystem();
			
	//Rotataion
	for(int i=0;i<3;i++){
		fRot[i] = 0.0f;
	}	
	fRot[0] = 20;
		
	//Flow manager
	mFlowMgr = new FlowManager();
	mFlowMgr->init();
		
	//Camera
	camSetPos(0, 0, SLAB_SIZE);//22
	camLookAt(0,0,0); //y= -2.5
	endDrag();
	
	//gui
	initGUI();
				
	//Current time
	iLastFrameTicks = SDL_GetTicks();
			
	//At this point, all the setup should be done
	LOG("Loaded, about to go to eventLoop\n");	

		
	//And hand off control to the event loop
	utilEventLoop();
					
	utilShutdown(0);
		
	return 0;
}

/*********************************************
		Exits the program, cleanup
**********************************************/
void App::utilShutdown( int r )
{
	LOG("Shutting down with returnCode %d\n", r);
	
	//Terminate the network connection
	closeSocket();
    
	LOG("Shutting down PS (of type %d)\n", mParticleSystem->getType());
		   
	//Shut down the particle system
	if(mParticleSystem){
		mParticleSystem->shutdown();
		delete mParticleSystem;
	}
	
	LOG("Freeing textures\n");
		   
	//Free textures
	texShutdown();
	
	//Clean up some other components
	mFlowMgr->shutdown();
	delete mFlowMgr;
	
	//This will ensure we break out of the eventloop
	done = true;    
	
	//Shut down SDL
    SDL_Quit( );   
    
    //So main() knows what to return
    returnCode = r;
}


/*********************************************
				Entry
**********************************************/
int main( int argc, char **argv )
{		
	App *a = new App();
	a->init(a, argc, argv);	
	delete a;
	
	printf("Goodbye\n");
		
	return returnCode;
}
