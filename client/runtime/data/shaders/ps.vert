uniform float fTime;
uniform float fSlabOffset;

void main()
{	
	gl_TexCoord[0] = gl_MultiTexCoord0;
	
	float pointSize = gl_TexCoord[0].y;
	float progress = (fTime - gl_TexCoord[0].x);	
				
	vec3 offset = gl_Normal * (fTime - gl_TexCoord[0].x);	
	vec4 offset4 = vec4(offset[0], offset[1], offset[2], 0.0);
	
	float xVal = gl_Vertex.x + offset.x;
	
	if(xVal < -fSlabOffset || xVal > fSlabOffset){
		pointSize = 0.0;
		offset.x = 99999.0;
	}
		
	gl_Position = gl_ModelViewProjectionMatrix * (gl_Vertex + offset4);
	gl_PointSize = pointSize;
						
	gl_FrontColor = gl_Color;	
} 

