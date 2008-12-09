#include "main.h"

//Adapted from NeHe


/*********************************************
				Globals
**********************************************/
static SDL_Surface *surface; //used for the screen
static int videoFlags;
static int isActive = true;
static SDL_Event event;

/*********************************************
		Begins a render cycle
**********************************************/
void App::utilBeginRender(){
	//cls
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	
	float ar = (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT;

	gluPerspective(70.0f,ar,0.5f,1000.0f);
	
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    glEnable(GL_DEPTH_TEST);
	
    	
	///////////////////////////////////////
	//			NOW IN 3D
	//////////////////////////////////////
	
}

/*********************************************
			Ends a render cycle
**********************************************/
void App::utilEndRender(){


	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	//Make it ortho
	//(0,0) == top-left
	glOrtho(0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_DEPTH_TEST);

	//now in 2D mode
	render2D();
			
	SDL_GL_SwapBuffers( );
}

/*********************************************
		Exits the program, cleanup
**********************************************/
void App::utilShutdown( int returnCode )
{
	LOG("Shutting down with returnCode %d\n", returnCode);

	//Get rid of the particle updating thread	
	SDL_KillThread(mParticleThread);
	
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
	
	LOG("All done, about to quit!\n");
	
	done = true;    
	
    SDL_Quit( );   
    exit( returnCode );    
    
    //todo: more cleanup here?    

}


/*********************************************
		Resets viewpoint after resize event
**********************************************/
static int resizeWindow( int width, int height )
{
	// Height / width ration 
	GLfloat ratio;

	// Protect against a divide by zero
	if ( height == 0 )
		height = 1;

	ratio = ( GLfloat )width / ( GLfloat )height;

	//Setup our viewport.
	glViewport( 0, 0, ( GLsizei )width, ( GLsizei )height );

	//change to the projection matrix and set our viewing volume.
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity( );
	
	//Set our perspective
//	gluPerspective( 45.0f, ratio, 0.1f, 100.0f );
	gluPerspective(90.0f,ratio,1.0f,10000.0f);

	// Make sure we're chaning the model view and not the projection 
	glMatrixMode( GL_MODELVIEW );

	//Reset The View 
	glLoadIdentity( );
	
	App::S()->iScreenX = width;
	App::S()->iScreenY = height;	

	return true;
}


/*********************************************
		Called on key-down
**********************************************/
void App::handleKeyEvent( SDL_keysym *keysym , int type )
{
	switch ( keysym->sym )
	{
	case SDLK_ESCAPE:
		//ESC key was pressed 
		utilShutdown( 0 );
		break;
	case SDLK_F1:
		//Toggle fullscreen
		SDL_WM_ToggleFullScreen( surface );
		break;
		
	default:
		break;
	}
		
	keySetState(keysym->sym, type == SDL_KEYDOWN);		
	onKeyEvent(keysym->unicode, type);		
	
	return;
}


/*********************************************
		General GL init
**********************************************/
static int initGL( )
{
    //Enable smooth shading 
    glShadeModel( GL_SMOOTH );

    //Set the background black
    glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );

    //Depth buffer setup
    glClearDepth( 1.0f );

    //Enables Depth Testing 
    glEnable( GL_DEPTH_TEST );

    //The Type Of Depth Test To Do 
    glDepthFunc( GL_LEQUAL );

    // Really Nice Perspective Calculations 
    glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

    return true;
}



/*********************************************
	Creates a window with specified attribs
**********************************************/
bool App::utilCreateWindow(int sizeX, int sizeY, int bpp, bool fullscreen){
 

	//this holds some info about our display
	const SDL_VideoInfo *videoInfo;
		//initialize SDL 
	if ( SDL_Init( SDL_INIT_VIDEO ) < 0 ){
		LOG( "Video initialization failed: %s\n", SDL_GetError( ) );
		return false;
	}

	SDL_EnableUNICODE(1);

	// Fetch the video info
	videoInfo = SDL_GetVideoInfo( );

	if ( !videoInfo ){
		LOG( "Video query failed: %s\n", SDL_GetError( ) );
		return false;
	}

	//the flags to pass to SDL_SetVideoMode
	videoFlags  = SDL_OPENGL;         
	videoFlags |= SDL_GL_DOUBLEBUFFER;
	videoFlags |= SDL_HWPALETTE;     
	videoFlags |= SDL_RESIZABLE;     

	//This checks to see if surfaces can be stored in memory 
	if ( videoInfo->hw_available )
		videoFlags |= SDL_HWSURFACE;
	else
		videoFlags |= SDL_SWSURFACE;

	// This checks if hardware blits can be done 
	if ( videoInfo->blit_hw )
		videoFlags |= SDL_HWACCEL;
	
	//Sets up OpenGL double buffering
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

	// Fullscreen?
	if(fullscreen){
		videoFlags |= SDL_FULLSCREEN;
	}

	// get a SDL surface 

	surface = SDL_SetVideoMode( iScreenX, iScreenY, bpp, videoFlags );

	// Verify there is a surface 
	if ( !surface ){
		LOG( "Video mode set failed: %s\n", SDL_GetError( ) );
		return false;
	}

	//Set the window caption
	SDL_WM_SetCaption( "BSOD Client", NULL );


	//initialize OpenGL 
	if(!initGL( )){
		return false;
	}

	//resize the initial window   
	resizeWindow( sizeX, sizeY );
	
	LOG("Made a %d/%d window @ %d bpp\n", sizeX, sizeY, bpp );
	
	return true;
}

/*********************************************
		SDL event loop
**********************************************/
void App::utilEventLoop(){

    while ( !done ){
	   
	    while ( SDL_PollEvent( &event ) ){
		    switch( event.type ){
			case SDL_ACTIVEEVENT:
			    /* Something's happend with our focus
			     * If we lost focus or we are iconified, we
			     * shouldn't draw the screen
			     */
			     
			     /*
			    if ( event.active.gain == 0 )
				isActive = false;
			    else
				isActive = true;*/
			    break;	
			      
			case SDL_VIDEORESIZE:
			    /* handle resize event */
			    surface = SDL_SetVideoMode( event.resize.w, event.resize.h, 16, videoFlags );
			    if ( !surface )
				{
				    fprintf( stderr, "Could not get a surface after resize: %s\n", SDL_GetError( ) );
				    utilShutdown( 1 );
				}
			    resizeWindow( event.resize.w, event.resize.h );
			    break;
			case SDL_KEYDOWN:
			    //mUIRoot.event(EVENT_KEY_DOWN, event.key.keysym.sym, iMouseX, iMouseY);
			    break;
			case SDL_KEYUP:
			    //mUIRoot.event(EVENT_KEY_UP, event.key.keysym.sym, iMouseX, iMouseY);
			    break;
			case SDL_QUIT:
			    /* handle quit requests */
			    done = true;
			    break;
			
			case SDL_MOUSEBUTTONDOWN:
				//onMouseEvent(event.button.button, EVENT_MOUSE_DOWN);
				//mUIRoot.event(EVENT_MOUSE_DOWN, event.button.button, iMouseX, iMouseY);
				beginDrag();
				break;	
				
			case SDL_MOUSEBUTTONUP:
				//onMouseEvent(event.button.button, EVENT_MOUSE_UP);
				//mUIRoot.event(EVENT_MOUSE_UP, event.button.button, iMouseX, iMouseY);
				//mUIRoot.event(EVENT_MOUSE_CLICK, event.button.button, iMouseX, iMouseY);
				endDrag();
				break;	
			
			default:
			    break;
			}
		}

	    /* draw the scene */
	    if ( isActive && !done){
			renderMain();
			updateMain();
		}
	}
	
}



/*********************************************
		Draws a box in 3space
**********************************************/
void App::utilCube(float x, float y, float z){

	x/=2; y/=2; z/=2;

	glBegin(GL_QUADS);
	// Front Face
		glNormal3f( 0.0f, 0.0f, 1.0f);					// Normal Pointing Towards Viewer
		glTexCoord2f(0.0f, 0.0f); glVertex3f(-x, -y,  z);	// Point 1 (Front)
		glTexCoord2f(1.0f, 0.0f); glVertex3f( x, -y,  z);	// Point 2 (Front)
		glTexCoord2f(1.0f, 1.0f); glVertex3f( x,  y,  z);	// Point 3 (Front)
		glTexCoord2f(0.0f, 1.0f); glVertex3f(-x,  y,  z);	// Point 4 (Front)
		// Back Face
		glNormal3f( 0.0f, 0.0f,-1.0f);					// Normal Pointing Away From Viewer
		glTexCoord2f(1.0f, 0.0f); glVertex3f(-x, -y, -z);	// Point 1 (Back)
		glTexCoord2f(1.0f, 1.0f); glVertex3f(-x,  y, -z);	// Point 2 (Back)
		glTexCoord2f(0.0f, 1.0f); glVertex3f( x,  y, -z);	// Point 3 (Back)
		glTexCoord2f(0.0f, 0.0f); glVertex3f( x, -y, -z);	// Point 4 (Back)
		// Top Face
		glNormal3f( 0.0f, 1.0f, 0.0f);					// Normal Pointing Up
		glTexCoord2f(0.0f, 1.0f); glVertex3f(-x,  y, -z);	// Point 1 (Top)
		glTexCoord2f(0.0f, 0.0f); glVertex3f(-x,  y,  z);	// Point 2 (Top)
		glTexCoord2f(1.0f, 0.0f); glVertex3f( x,  y,  z);	// Point 3 (Top)
		glTexCoord2f(1.0f, 1.0f); glVertex3f( x,  y, -z);	// Point 4 (Top)
		// Bottom Face
		glNormal3f( 0.0f,-1.0f, 0.0f);					// Normal Pointing Down
		glTexCoord2f(1.0f, 1.0f); glVertex3f(-x, -y, -z);	// Point 1 (Bottom)
		glTexCoord2f(0.0f, 1.0f); glVertex3f( x, -y, -z);	// Point 2 (Bottom)
		glTexCoord2f(0.0f, 0.0f); glVertex3f( x, -y,  z);	// Point 3 (Bottom)
		glTexCoord2f(1.0f, 0.0f); glVertex3f(-x, -y,  z);	// Point 4 (Bottom)
		// Right face
		glNormal3f( 1.0f, 0.0f, 0.0f);					// Normal Pointing Right
		glTexCoord2f(1.0f, 0.0f); glVertex3f( x, -y, -z);	// Point 1 (Right)
		glTexCoord2f(1.0f, 1.0f); glVertex3f( x,  y, -z);	// Point 2 (Right)
		glTexCoord2f(0.0f, 1.0f); glVertex3f( x,  y,  z);	// Point 3 (Right)
		glTexCoord2f(0.0f, 0.0f); glVertex3f( x, -y,  z);	// Point 4 (Right)
		// Left Face
		glNormal3f(-1.0f, 0.0f, 0.0f);					// Normal Pointing Left
		glTexCoord2f(0.0f, 0.0f); glVertex3f(-x, -y, -z);	// Point 1 (Left)
		glTexCoord2f(1.0f, 0.0f); glVertex3f(-x, -y,  z);	// Point 2 (Left)
		glTexCoord2f(1.0f, 1.0f); glVertex3f(-x,  y,  z);	// Point 3 (Left)
		glTexCoord2f(0.0f, 1.0f); glVertex3f(-x,  y, -z);	// Point 4 (Left)
	glEnd();								// Done Drawing Quads
}


float dotProduct(float x, float y, float z, float x2, float y2, float z2){
	return x*x2 + y*y2 + z*z2;
}

/*********************************************
	Turns the 2D mouse coords into a 3D
	intersection point of the plane
**********************************************/
void App::calculateMousePoint(){
	
	//First get the mouse point
	int x, y;
	SDL_GetMouseState(&x, &y); 
		
	//Temp storage
	GLint viewport[4];
	GLdouble modelview[16];
	GLdouble projection[16];
	GLfloat winX, winY, winZ;
	GLdouble posX, posY, posZ;

	//Get the various matrixes to put into gluUnProject
	glGetDoublev( GL_MODELVIEW_MATRIX, modelview );
	glGetDoublev( GL_PROJECTION_MATRIX, projection );
	glGetIntegerv( GL_VIEWPORT, viewport );

	winX = (float)x;
	winY = (float)viewport[3] - (float)y; //GL inverts Y, so fix that
	
	//Get the depth
	glReadPixels( x, int(winY), 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &winZ );
	
	//Unproject. This gives us the point that the mouse is hovered over (in 3space)
	//Now we throw some maths at it and it will give us the intersection point with the 
	//plane of hexes
	gluUnProject( winX, winY, winZ, modelview, projection, viewport, &posX, &posY, &posZ);
	
	fMouseX = posX;
	fMouseY = posY;
	fMouseZ = posZ;		
}

/*********************************************
	Turns a 3D point into a 2D screen pos
**********************************************/
Vector2 App::utilProject(float x, float y, float z){
	GLdouble pX, pY, pZ;
	
	// arrays to hold matrix information

	GLdouble model_view[16];
	glGetDoublev(GL_MODELVIEW_MATRIX, model_view);

	GLdouble projection[16];
	glGetDoublev(GL_PROJECTION_MATRIX, projection);

	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	
	// get 3D coordinates based on window coordinates

	gluProject(x, y, z,
		model_view, projection, viewport,
		&pX, &pY, &pZ);
			
	return Vector2(pX, SCREEN_HEIGHT - pY);
}

/*********************************************
				Draws the plane
**********************************************/
void App::utilPlane(float x, float y, float z){

	x/=2; y/=2; z/=2;
		
	glEnable(GL_TEXTURE_2D);

	glBegin(GL_QUADS);
	// Front Face
		glNormal3f( 0.0f, 0.0f, 1.0f);					// Normal Pointing Towards Viewer
		glTexCoord2f(0.0f, 0.0f); glVertex3f(-x, -y,  z);	// Point 1 (Front)
		glTexCoord2f(1.0f, 0.0f); glVertex3f( x, -y,  z);	// Point 2 (Front)
		glTexCoord2f(1.0f, -1.0f); glVertex3f( x,  y,  z);	// Point 3 (Front)
		glTexCoord2f(0.0f, -1.0f); glVertex3f(-x,  y,  z);	// Point 4 (Front)		
	glEnd();								// Done Drawing Quads

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
	glColor3f(0.25f, 0.25f, 0.25f);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	
	glBegin(GL_QUADS);
	// Front Face
		glNormal3f( 0.0f, 0.0f, 1.0f);					// Normal Pointing Towards Viewer
		glVertex3f(-x, -y,  z);	// Point 1 (Front)
		glVertex3f( x, -y,  z);	// Point 2 (Front)
		glVertex3f( x,  y,  z);	// Point 3 (Front)
		glVertex3f(-x,  y,  z);	// Point 4 (Front)		
	glEnd();								// Done Drawing Quads
	
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);

	
}
