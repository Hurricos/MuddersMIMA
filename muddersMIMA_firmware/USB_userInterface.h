//Copyright 2022-2023(c) John Sullivan


#ifndef USB_userInterface_h
	#define USB_userInterface_h
	
	#define USER_INPUT_BUFFER_SIZE 32
	#define STRING_TERMINATION_CHARACTER 0

	#define INPUT_FLAG_INSIDE_COMMENT 0x01

	uint8_t USB_userInterface_getUserInput(void);

	void USB_userInterface_handler(void);

#endif