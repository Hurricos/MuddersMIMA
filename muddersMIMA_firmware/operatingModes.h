//Copyright 2022-2023(c) John Sullivan


#ifndef modes_h
	#define modes_h

	#define FAS_MAX_STOPPED_RPM			500
	void operatingModes_handler(void);
    uint8_t evaluate_2d_map(uint8_t* map, uint8_t* dim1_map, uint8_t* dim2_map, uint8_t dim1_size, uint8_t dim2_size, uint8_t dim1, uint8_t dim2);
    uint8_t operatingModes_getLastUsedMapIndex();
#endif
