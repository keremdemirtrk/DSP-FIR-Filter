#include <stdio.h>
#include <stdint.h>
#define _CRT_SECURE_NO_WARNINGS
#define PI 3.14f
#define WAV_HEADER_LENGTH 44
#define L2B_ENDIAN(n) (((n) >> 24)&0xff) | (((n) << 8) & 0xff0000) | (((n) >> 8) & 0xff00) | (((n) << 24) & 0xff000000)

 //////////////////////////////////////////////////////////////
// Filter Code Definitions
//////////////////////////////////////////////////////////////
// maximum number of inputs that can be handled
// in one function call
#define MAX_INPUT_LEN 80
// maximum length of filter than can be handled
#define MAX_FLT_LEN 63
// buffer to hold all of the input samples
#define BUFFER_LEN 142
// array to hold input samples
double insamp[BUFFER_LEN];

char* read_wav(const char* filename, short* nchannel, short* ssample, int* csample) {

	// OPEN THE FILE

	FILE* fp = fopen(filename, "rb");

	if (!fp) {
		fprintf(stderr, "Couldn't open the file \"%s\"\n", filename);
        return NULL;
	}

	fseek(fp, 0, SEEK_END);
	int file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	printf("The file \"%s\" has %d bytes\n\n", filename, file_size);

	char* buffer = (char*)malloc(sizeof(char) * file_size);
	fread(buffer, file_size, 1, fp);

	// DUMP THE buffer INFO

	*nchannel = *(short*)&buffer[22];
	*ssample = *(short*)&buffer[34] / 8;
	*csample = *(int*)&buffer[40] / *ssample;

	printf("ChunkSize :\t %u\n", *(int*)&buffer[4]);
	printf("Format :\t %u\n", *(short*)&buffer[20]);
	printf("NumChannels :\t %u\n", *(short*)&buffer[22]);
	printf("SampleRate :\t %u\n", *(int*)&buffer[24]);	// number of samples per second
	printf("ByteRate :\t %u\n", *(int*)&buffer[28]);		// number of bytes per second
	printf("BitsPerSample :\t %u\n", *(short*)&buffer[34]);
	printf("Subchunk2ID :\t \"%c%c%c%c\"\n", buffer[36], buffer[37], buffer[38], buffer[39]);	// marks beginning of the data section
	printf("Subchunk2Size :\t %u\n", *(int*)&buffer[40]);		// size of data (byte)
	printf("Duration :\t %fs\n\n", (float)(*(int*)&buffer[40]) / *(int*)&buffer[28]);

	fclose(fp);
	return buffer;
}

void write_wav(const char* filename, const char* data, int len) {
	FILE* fp = fopen(filename, "wb");

	if (!fp) {
		fprintf(stderr, "Couldn't open the file \"%s\"\n", filename);
		exit(0);
	}

	fwrite(data, len, 1, fp);
	fclose(fp);
}

// FIR init
void firFloatInit(void) {
  memset(insamp, 0, sizeof(insamp));
}

// the FIR filter function
void firFloat(double * coeffs, double * input, double * output,  int length, int filterLength) {
  double acc; // accumulator for MACs
  double * coeffp; // pointer to coefficients
  double * inputp; // pointer to input samples
  int n;
  int k;
  // put the new samples at the high end of the buffer
  memcpy( & insamp[filterLength - 1], input,
    length * sizeof(double));
  // apply the filter to each input sample
  for (n = 0; n < length; n++) {
    // calculate output n
    coeffp = coeffs;
    inputp = & insamp[filterLength - 1 + n];
    acc = 0;
    for (k = 0; k < filterLength; k++) {
      acc += ( * coeffp++) * ( * inputp--);
    }
    output[n] = acc;
  }
  // shift input samples back in time for next time
  memmove( & insamp[0], & insamp[length],
    (filterLength - 1) * sizeof(double));
}
//////////////////////////////////////////////////////////////
// Test program
//////////////////////////////////////////////////////////////
// bandpass filter centred around 1000 Hz
// sampling rate = 8000 Hz
#define FILTER_LEN 63
double coeffs[FILTER_LEN] = {
0.000083, 
0.000291, 
-0.000322, 
-0.000490, 
0.000805, 
0.000588, 
-0.001588, 
-0.000407, 
0.002659, 
-0.000280, 
-0.003906, 
0.001720, 
0.005084, 
-0.004127, 
-0.005800, 
0.007630, 
0.005518, 
-0.012225, 
-0.003565, 
0.017738, 
-0.000887, 
-0.023824, 
0.008942, 
0.029988, 
-0.022444, 
-0.035649, 
0.045741, 
0.040217, 
-0.094717, 
-0.043187, 
0.314404, 
0.544218, 
0.314404, 
-0.043187, 
-0.094717, 
0.040217, 
0.045741, 
-0.035649, 
-0.022444, 
0.029988, 
0.008942, 
-0.023824, 
-0.000887, 
0.017738, 
-0.003565, 
-0.012225, 
0.005518, 
0.007630, 
-0.005800, 
-0.004127, 
0.005084, 
0.001720, 
-0.003906, 
-0.000280, 
0.002659, 
-0.000407, 
-0.001588, 
0.000588, 
0.000805, 
-0.000490, 
-0.000322, 
0.000291, 
0.000083
};

void intToFloat(int16_t * input, double * output, int length) {
  int i;
  for (i = 0; i < length; i++) {
    output[i] = (double) input[i];
  }
}

void floatToInt(double * input, int16_t * output, int length) {
  int i;
  for (i = 0; i < length; i++) {
    // add rounding constant
    input[i] += 0.5;
    // bound the values to 16 bits
    if (input[i] > 32767.0) {
      input[i] = 32767.0;
    } else if (input[i] < -32768.0) {
      input[i] = -32768.0;
    }
    // convert
    output[i] = (int16_t) input[i];
  }
}

// number of samples to read per loop
#define SAMPLES 80
int main(void) {
  int size;
  int16_t input[SAMPLES];
  int16_t output[SAMPLES];
  double floatInput[SAMPLES];
  double floatOutput[SAMPLES];
  FILE * in_fid;
  FILE * out_fid;
  FILE * data_infid;
  data_infid = fopen("input.pcm", "wb");

  // open the input waveform file
  short nchannel, ssample;
  int csample;
  char* buffer = read_wav("preamble.wav", &nchannel, &ssample, &csample);
  printf("size: %d\n", csample*ssample);

  short* data = (short*)&buffer[44];

  fwrite(data, csample*ssample, 1, data_infid);
  fclose(data_infid);
  in_fid = fopen("input.pcm", "rb");
  if (in_fid == 0) {
    printf("couldn't open input.pcm");
    return;
  }
  // open the output waveform file
  out_fid = fopen("outputFloat.pcm", "wb");
  if (out_fid == 0) {
    printf("couldn't open outputFloat.pcm");
    return;
  }
  fwrite(buffer, 44, 1, out_fid);
  // initialize the filter
  firFloatInit();
  // process all of the samples
  do {
    // read samples from file
    size = fread(input, sizeof(int16_t), SAMPLES, in_fid);
    // convert to doubles
    intToFloat(input, floatInput, size);
    // perform the filtering
    firFloat(coeffs, floatInput, floatOutput, size,
      FILTER_LEN);
    // convert to ints
    floatToInt(floatOutput, output, size);
    // write samples to file
    fwrite(output, sizeof(int16_t), size, out_fid);
  } while (size != 0);
  //fclose(in_fid);
  fclose(out_fid);
  return 0;
}