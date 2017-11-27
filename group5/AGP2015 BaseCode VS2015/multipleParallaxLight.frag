#version 330 core

// Authors : Matteo Marcuzzo, Meshal Marakkar

struct Material {
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
}; 


struct PointLight {
    vec3 position;
    
    float constant;
    float linear;
    float quadratic;
	
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform int numShotsFired;
#define NR_POINT_LIGHTS 4
float heightScale = 0.1f; //control extent of occlusion

in vec3 FragPos;

in vec2 TexCoords;
in vec3 tangentViewPos;
in vec3 tangentFragPos;
in mat3 TBN;

in vec3 worldNormal;
in vec3 worldTangent;
in vec3 bitangent;

float parallaxHeight;
vec2 texStep;

layout(location = 0) out vec4 out_Color;

uniform vec3 viewPos;
uniform PointLight pointLights[NR_POINT_LIGHTS];
uniform Material material;

uniform sampler2D diffuseMap;
uniform sampler2D heightMap;
uniform sampler2D normalMap;
uniform bool parallax;

uniform float far_plane;

uniform samplerCube depthMap[NR_POINT_LIGHTS];

vec3 sampleOffsetDirections[20] = vec3[]
(
   vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
   vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
   vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
   vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
   vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);   

float ShadowCalculation(vec3 fragPos, vec3 lightPosition, samplerCube shadowCube)
{

    // Get vector between fragment position and light position
    vec3 fragToLight = fragPos - lightPosition;
    // Use the light to fragment vector to sample from the depth map    
    float closestDepth = texture(shadowCube, fragToLight).r;
    // It is currently in linear range between [0,1]. Re-transform back to original value
    closestDepth *= far_plane;
    // Now get current linear depth as the length between the fragment and light position
    float currentDepth = length(fragToLight);
    // Now test for shadows
    float shadow = 0.0;
	float bias = 0.15;
	int samples = 20;
	float viewDistance = length(viewPos - fragPos);
	float diskRadius = (1.0 + (viewDistance / far_plane)) / 25.0;  
	for(int i = 0; i < samples; ++i)
	{
		float closestDepth = texture(shadowCube, fragToLight + sampleOffsetDirections[i] * diskRadius).r;
		closestDepth *= far_plane;   // Undo mapping [0;1]
		if(currentDepth - bias > closestDepth)
			shadow += 1.0;
	}
	shadow /= float(samples);  

    return shadow;
}

// Function prototypes
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 colour, vec2 newTexCoords, float shadow, float shadowMultiplier);
vec2 ParallaxMapping(vec2 newTexCoords, vec3 viewDir);
float parallaxSoftShadowMultiplier(in vec3 L, in vec2 initialTexCoord,
                                       in float initialHeight);

void main()
{    
    // Properties
    vec3 viewDir = normalize(tangentViewPos - tangentFragPos);
	vec2 newTexCoords = TexCoords;
    
	if (parallax)
    newTexCoords = ParallaxMapping(TexCoords,  viewDir);
        
    // discards a fragment when sampling outside default texture region (fixes border artifacts)
    if(newTexCoords.x > 1.0f || newTexCoords.y > 1.0f || newTexCoords.x < 0.0f || newTexCoords.y < 0.0f)
        discard;
	vec3 normal = normalize( (texture( normalMap, newTexCoords).rgb-0.5) * 2 );
	vec3 colour = texture(diffuseMap, newTexCoords).rgb;
    float shadow = 0.0;
    vec3 result;
    // Point lights
    for(int i = 0; i < NR_POINT_LIGHTS; i++){
	
	vec3 worldDirectionToLight	= normalize(pointLights[i].position - FragPos);

    // transform direction to the light to tangent space
    vec3 toLightInTangentSpace =  normalize( vec3( dot(worldDirectionToLight, worldTangent),
													dot(worldDirectionToLight, bitangent),
													dot(worldDirectionToLight, worldNormal)));

	// get self-shadowing factor for elements of parallax
    float shadowMultiplier = parallaxSoftShadowMultiplier(toLightInTangentSpace, newTexCoords, parallaxHeight - 0.05);

		shadow = ShadowCalculation(FragPos, pointLights[i].position, depthMap[i]);
        result += CalcPointLight(pointLights[i], normal, FragPos, viewDir, colour, newTexCoords, shadow, shadowMultiplier);    
	}

    out_Color = vec4(result, 1.0);
}

// Calculates the color when using a point light.
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 colour, vec2 newTexCoords, float shadow, float shadowMultiplier)
{  
	// Ambient shading
	vec3 ambient = light.ambient * colour;
	
	// Diffuse shading
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(viewDir, lightDir), 0.0);
	vec3 diffuse = light.diffuse * diff * colour;

    // Specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
	//float spec = pow(max(dot(normal, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * spec * vec3(texture(material.specular, newTexCoords));

	// Attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0f / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
    
	// Combine results
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    return (ambient + (diffuse + specular) * (1.0 - shadow) * pow(shadowMultiplier, 4));
}

vec2 ParallaxMapping(vec2 newTexCoords, vec3 viewDir){
 
	// number of depth layers. Increase layers for cleaner look
    const float minLayers = 10.0f;
    const float maxLayers = 30.0f;

	//mix - linearly interpolate. 
	//abs - returns absolute value of parameter (magnitude of real no. regardless of relation to other values)
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir))); 

    // calculate the size of each layer
    float layerDepth = 1.0f / numLayers;

    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy / viewDir.z * heightScale; 
	//vec2 P = viewDir.xy * heightScale; //parallax with offset limiting
    vec2 deltaTexCoords = P / numLayers;

      // depth of current layer
    float currentLayerDepth = 0.0f;

    // get initial values
    vec2  currentTexCoords = newTexCoords;
    float currentDepthMapValue = texture(heightMap, currentTexCoords).r;
      
	while(currentLayerDepth < currentDepthMapValue)
    {
        // shift texture coordinates along direction of P
        currentTexCoords -= deltaTexCoords;

        // get depthmap value at current texture coordinates
        currentDepthMapValue = texture(heightMap, currentTexCoords).r;

        // get depth of next layer
        currentLayerDepth += layerDepth;  
    }
    
    // -- parallax occlusion mapping interpolation from here on
    // get texture coordinates before collision (reverse operations)
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

    // get depth after and before collision for linear interpolation
    float afterDepth  = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = texture(heightMap, prevTexCoords).r - currentLayerDepth + layerDepth;
	
    // interpolation of texture coordinates
    float weight = afterDepth / (afterDepth - beforeDepth);
	parallaxHeight = currentLayerDepth + beforeDepth * weight + afterDepth * (1.0 - weight);

    vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

    return finalTexCoords;
}

float parallaxSoftShadowMultiplier(in vec3 L, in vec2 initialTexCoord,
                                       in float initialHeight)
{
   float shadowMultiplier = 1;

   const float minLayers = 15;
   const float maxLayers = 30;

   // calculate lighting only for surface oriented to the light source - PROBLEM???????
   if(dot(vec3(0, 0, 1), L) > 0)
   {
      // calculate initial parameters
      float numSamplesUnderSurface	= 0;
      shadowMultiplier	= 0;
      float numLayers	= mix(maxLayers, minLayers, abs(dot(vec3(0, 0, 1), L)));
      float layerHeight	= initialHeight / numLayers;
      texStep	= heightScale * L.xy / L.z / numLayers;

      // current parameters
      float currentLayerHeight	= initialHeight - layerHeight;
      vec2 currentTextureCoords	= initialTexCoord + texStep;
      float heightFromTexture	= texture(heightMap, currentTextureCoords).r;
      int stepIndex	= 1;

      // while point is below depth 0.0 )
      while(currentLayerHeight > 0)
      {
         // if point is under the surface
         if(heightFromTexture < currentLayerHeight)
         {
            // calculate partial shadowing factor
            numSamplesUnderSurface	+= 1;
            float newShadowMultiplier	= (currentLayerHeight - heightFromTexture) *
                                             (1.0 - stepIndex / numLayers);
            shadowMultiplier	= max(shadowMultiplier, newShadowMultiplier);
         }

         // offset to the next layer
         stepIndex	+= 1;
         currentLayerHeight	-= layerHeight;
         currentTextureCoords	+= texStep;
         heightFromTexture	= texture(heightMap, currentTextureCoords).r;
      }

      // Shadowing factor should be 1 if there were no points under the surface
      if(numSamplesUnderSurface < 1)
      {
         shadowMultiplier = 1;
      }
      else
      {
         shadowMultiplier = 1.0 - shadowMultiplier;
      }
   }
   return shadowMultiplier;
}

