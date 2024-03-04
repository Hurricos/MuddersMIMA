//Copyright 2024(c) Martin Kennedy

#ifndef twod_map_h
	#define twod_map_h

	uint8_t evaluate_2d_map(uint8_t* map, uint8_t* dim1_map, uint8_t dim1_size, uint8_t dim1, uint8_t* dim2_map, uint8_t dim2_size, uint8_t dim2);
	uint8_t evaluate_3d_map(uint8_t* map, uint8_t* dim1_map, uint8_t dim1_size, uint8_t dim1, uint8_t* dim2_map, uint8_t dim2_size, uint8_t dim2, uint8_t* dim3_map, uint8_t dim3_size, uint8_t dim3);
    uint8_t two_d_map_getLastUsedMapIndex();
#endif
