#include <kos.h>

#include <mp3/sndserver.h>
#include <plx/matrix.h>
#include <plx/prim.h>

#include "lrrsoft.h"

// Initialize KOS
KOS_INIT_FLAGS(INIT_DEFAULT);

// Initialize the ROM disk
extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);

// Global variables
bool exitProgram = false;
pvr_poly_hdr_t nontexturedHeader;

float rotationX = 0;
float rotationY = 0;

pvr_ptr_t texMemory[6];
pvr_poly_hdr_t texHeaders[6];

float ambientLight = 0.3f;
float diffuseLight = 0.7f;
vector_t lightPosition = { 0, 0, 10, 1 };

vector_t verts[4] = {
	{ -1, 1, 0, 1 },
	{ -1, -1, 0, 1 },
	{ 1, 1, 0, 1 },
	{ 1, -1, 0, 1}
};

vector_t verts_in[8] = {
	{  1.0f,  1.0f,  1.0f, 1.0f },
	{ -1.0f,  1.0f,  1.0f, 1.0f },
	{  1.0f, -1.0f,  1.0f, 1.0f },
	{ -1.0f, -1.0f,  1.0f, 1.0f },
	{  1.0f,  1.0f, -1.0f, 1.0f },
	{ -1.0f,  1.0f, -1.0f, 1.0f },
	{  1.0f, -1.0f, -1.0f, 1.0f },
	{ -1.0f, -1.0f, -1.0f, 1.0f }
};

vector_t normals[6] = {
	{  0.0f,  0.0f,  1.0f, 0.0f },
	{  0.0f,  0.0f, -1.0f, 0.0f },
	{  1.0f,  0.0f,  0.0f, 0.0f },
	{ -1.0f,  0.0f,  0.0f, 0.0f },
	{  0.0f,  1.0f,  0.0f, 0.0f },
	{  0.0f, -1.0f,  0.0f, 0.0f }
};

pvr_ptr_t loadTexture(const char *texName)
{
	FILE *texFile = fopen(texName, "rb");
	pvr_ptr_t textureMemory = pvr_mem_malloc(174768);
	char header[32];
	fread(header, 1, 32, texFile);
	fread(textureMemory, 1, 174768, texFile);
	fclose(texFile);

	return textureMemory;
}

pvr_poly_hdr_t createTexHeader(pvr_ptr_t texture)
{
	pvr_poly_cxt_t context;
	pvr_poly_cxt_txr(&context, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_TWIDDLED, 256, 256, texture, PVR_FILTER_NONE);

	context.gen.culling = PVR_CULLING_CW;
	context.txr.mipmap  = PVR_MIPMAP_ENABLE;

	pvr_poly_hdr_t header;
	pvr_poly_compile(&header, &context);
	return header;
}

float calculateDiffuseIntensity(vector_t light, vector_t point, vector_t normal)
{
	vec3f_normalize(normal.x, normal.y, normal.z);

	vector_t lightDirection;
	vec3f_sub_normalize(
		light.x, light.y, light.z,
		point.x, point.y, point.z,
		lightDirection.x, lightDirection.y, lightDirection.z
	);

	float intensity;
	vec3f_dot(
		normal.x, normal.y, normal.z,
		lightDirection.x, lightDirection.y, lightDirection.z,
		intensity
	);

	if (intensity > 0)
		return intensity;
	else
		return 0;
}

void submitVertex(vector_t light, vector_t lightVertex, vector_t vertex, vector_t normal, float u, float v, bool endOfStrip = false)
{
	int flags = endOfStrip ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;

	float intensity = calculateDiffuseIntensity(light, lightVertex, normal);
	float color = ambientLight + (intensity * diffuseLight);

	plx_vert_ffp(flags, vertex.x, vertex.y, vertex.z, 1.0f, color, color, color, u, v);
}

void Initialize()
{
	// Draw to the VMU
	maple_device_t *vmu = maple_enum_type(0, MAPLE_FUNC_LCD);
	vmu_draw_lcd(vmu, lrrsoft_logo);

	// Initialize the graphics libraries
	pvr_init_defaults();
	plx_mat3d_init();
	snd_stream_init();
	mp3_init();

	// Compile a polygon header
	pvr_poly_cxt_t nontexturedContext;
	pvr_poly_cxt_col(&nontexturedContext, PVR_LIST_OP_POLY);
	nontexturedContext.gen.culling = PVR_CULLING_CW;
	pvr_poly_compile(&nontexturedHeader, &nontexturedContext);

	// Load all 6 textures
	texMemory[0] = loadTexture("/rd/dclogo.pvr");
	texMemory[1] = loadTexture("/rd/lrrlogo.pvr");
	texMemory[2] = loadTexture("/rd/ihorner.pvr");
	texMemory[3] = loadTexture("/rd/gstar.pvr");
	texMemory[4] = loadTexture("/rd/turner.pvr");
	texMemory[5] = loadTexture("/rd/savidan.pvr");

	for (int i = 0; i < 6; ++i)
		texHeaders[i] = createTexHeader(texMemory[i]);

	// Set a background color
	pvr_set_bg_color(0.46f, 0.22f, 0.49f);

	// Set up the camera
	plx_mat3d_mode(PLX_MAT_PROJECTION);
	plx_mat3d_identity();
	plx_mat3d_perspective(60.0f, 640.0f / 480.0f, 0.1f, 100.0f);

	plx_mat3d_mode(PLX_MAT_MODELVIEW);
	plx_mat3d_identity();

	point_t cameraPosition = { 0.0f, 0.0f, 5.0f, 1.0f };
	point_t cameraTarget   = { 0.0f, 0.0f, 0.0f, 1.0f };
	vector_t cameraUp      = { 0.0f, 1.0f, 0.0f, 0.0f };
	plx_mat3d_lookat(&cameraPosition, &cameraTarget, &cameraUp);
	
	// Play music
	mp3_start("/rd/tucson.mp3", 1);
}

void Update()
{
	plx_mat3d_push();
	
	// Do lighting calculations
	plx_mat_identity();
	plx_mat3d_apply(PLX_MAT_MODELVIEW);

	// Transform light position
	vector_t light = lightPosition;
	mat_trans_single4(light.x, light.y, light.z, light.w);
	
	plx_mat3d_rotate(rotationX, 1, 0, 0);
	plx_mat3d_rotate(rotationY, 0, 1, 0);
	
	rotationX += 1.0;
	rotationY += 0.5;

	// Transform normals
	vector_t transformedNormals[6];
	for (int i = 0; i < 6; ++i)
	{
		mat_trans_normal3_nomod(
			normals[i].x, normals[i].y, normals[i].z,
			transformedNormals[i].x, transformedNormals[i].y, transformedNormals[i].z
		);
	}

	// Transform vertices into camera space
	vector_t lightVertices[8];
	plx_mat_transform(verts_in, lightVertices, 8, 4 * sizeof(float));

	// Transform vertices for graphics chip
	plx_mat_identity();
	plx_mat3d_apply_all();

	vector_t transformedVerts[8];
	plx_mat_transform(verts_in, transformedVerts, 8, 4 * sizeof(float));
	
	plx_mat3d_pop();

	// Wait for the PVR to accept a frame
	pvr_wait_ready();

	pvr_scene_begin();
	pvr_list_begin(PVR_LIST_OP_POLY);

	pvr_prim(&texHeaders[0], sizeof(pvr_poly_hdr_t));
	submitVertex(light, lightVertices[0], transformedVerts[0], transformedNormals[0], 1, 0);
	submitVertex(light, lightVertices[1], transformedVerts[1], transformedNormals[0], 0, 0);
	submitVertex(light, lightVertices[2], transformedVerts[2], transformedNormals[0], 1, 1);
	submitVertex(light, lightVertices[3], transformedVerts[3], transformedNormals[0], 0, 1, true);

	pvr_prim(&texHeaders[1], sizeof(pvr_poly_hdr_t));
	submitVertex(light, lightVertices[5], transformedVerts[5], transformedNormals[1], 1, 0);
	submitVertex(light, lightVertices[4], transformedVerts[4], transformedNormals[1], 0, 0);
	submitVertex(light, lightVertices[7], transformedVerts[7], transformedNormals[1], 1, 1);
	submitVertex(light, lightVertices[6], transformedVerts[6], transformedNormals[1], 0, 1, true);

	pvr_prim(&texHeaders[2], sizeof(pvr_poly_hdr_t));
	submitVertex(light, lightVertices[4], transformedVerts[4], transformedNormals[2], 1, 0);
	submitVertex(light, lightVertices[0], transformedVerts[0], transformedNormals[2], 0, 0);
	submitVertex(light, lightVertices[6], transformedVerts[6], transformedNormals[2], 1, 1);
	submitVertex(light, lightVertices[2], transformedVerts[2], transformedNormals[2], 0, 1, true);

	pvr_prim(&texHeaders[3], sizeof(pvr_poly_hdr_t));
	submitVertex(light, lightVertices[1], transformedVerts[1], transformedNormals[3], 1, 0);
	submitVertex(light, lightVertices[5], transformedVerts[5], transformedNormals[3], 0, 0);
	submitVertex(light, lightVertices[3], transformedVerts[3], transformedNormals[3], 1, 1);
	submitVertex(light, lightVertices[7], transformedVerts[7], transformedNormals[3], 0, 1, true);

	pvr_prim(&texHeaders[4], sizeof(pvr_poly_hdr_t));
	submitVertex(light, lightVertices[4], transformedVerts[4], transformedNormals[4], 1, 0);
	submitVertex(light, lightVertices[5], transformedVerts[5], transformedNormals[4], 0, 0);
	submitVertex(light, lightVertices[0], transformedVerts[0], transformedNormals[4], 1, 1);
	submitVertex(light, lightVertices[1], transformedVerts[1], transformedNormals[4], 0, 1, true);

	pvr_prim(&texHeaders[5], sizeof(pvr_poly_hdr_t));
	submitVertex(light, lightVertices[7], transformedVerts[7], transformedNormals[5], 1, 0);
	submitVertex(light, lightVertices[6], transformedVerts[6], transformedNormals[5], 0, 0);
	submitVertex(light, lightVertices[3], transformedVerts[3], transformedNormals[5], 1, 1);
	submitVertex(light, lightVertices[2], transformedVerts[2], transformedNormals[5], 0, 1, true);

	pvr_list_finish();
	pvr_scene_finish();
}

void Cleanup()
{
	// Clear the VMU screen
	maple_device_t *vmu = maple_enum_type(0, MAPLE_FUNC_LCD);
	vmu_draw_lcd(vmu, vmu_clear);
	
	pvr_mem_free(texMemory[5]);
	pvr_mem_free(texMemory[4]);
	pvr_mem_free(texMemory[3]);
	pvr_mem_free(texMemory[2]);
	pvr_mem_free(texMemory[1]);
	pvr_mem_free(texMemory[0]);

	mp3_stop();
	mp3_shutdown();
	snd_stream_shutdown();
	pvr_shutdown();
}

int main(int argc, char *argv[])
{
	Initialize();

	while (!exitProgram)
	{
		maple_device_t *controller = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
		cont_state_t *controllerState = reinterpret_cast<cont_state_t*>(maple_dev_status(controller));
		if (controllerState->buttons & CONT_START)
			exitProgram = true;

		Update();
	}

	Cleanup();

	return 0;
}
