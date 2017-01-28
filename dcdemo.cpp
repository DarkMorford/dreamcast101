#include <kos.h>

// Initialize KOS
KOS_INIT_FLAGS(INIT_DEFAULT);

// Initialize the ROM disk
extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);

// Global variables
bool exitProgram = false;

void Initialize()
{
}

void Update()
{
}

void Cleanup()
{
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
