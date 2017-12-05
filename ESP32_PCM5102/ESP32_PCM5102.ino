#include <mySD.h>
#include "driver/i2s.h"
#include "freertos/queue.h"

#define CCCC(c1, c2, c3, c4)    ((c4 << 24) | (c3 << 16) | (c2 << 8) | c1)

/* These are data structures to process wav file */
typedef enum headerState_e {
    HEADER_RIFF, HEADER_FMT, HEADER_DATA, DATA
} headerState_t;

typedef struct wavRiff_s {
    uint32_t chunkID;
    uint32_t chunkSize;
    uint32_t format;
} wavRiff_t;

typedef struct wavProperties_s {
    uint32_t chunkID;
    uint32_t chunkSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
} wavProperties_t;
/* variables hold file, state of process wav file and wav file properties */    
File root;
headerState_t state = HEADER_RIFF;
wavProperties_t wavProps;

//i2s configuration 
int i2s_num = 0; // i2s port number
i2s_config_t i2s_config = {
     .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
     .sample_rate = 36000,
     .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
     .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
     .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
     .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // high interrupt priority
     .dma_buf_count = 8,
     .dma_buf_len = 64   //Interrupt level 1
    };
    
i2s_pin_config_t pin_config = {
    .bck_io_num = 26, //this is BCK pin
    .ws_io_num = 25, // this is LRCK pin
    .data_out_num = 22, // this is DATA output pin
    .data_in_num = -1   //Not used
};
//
void debug(uint8_t *buf, int len){
  for(int i=0;i<len;i++){
    Serial.print(buf[i], HEX);
    Serial.print("\t");
  }
  Serial.println();
}
/* write sample data to I2S */
int i2s_write_sample_nb(uint32_t sample){
  return i2s_write_bytes((i2s_port_t)i2s_num, (const char *)&sample, sizeof(uint32_t), 100);
}
/* read 4 bytes of data from wav file */
int read4bytes(File file, uint32_t *chunkId){
  int n = file.read((uint8_t *)chunkId, sizeof(uint32_t));
  return n;
}

/* these are function to process wav file */
int readRiff(File file, wavRiff_t *wavRiff){
  int n = file.read((uint8_t *)wavRiff, sizeof(wavRiff_t));
  return n;
}
int readProps(File file, wavProperties_t *wavProps){
  int n = file.read((uint8_t *)wavProps, sizeof(wavProperties_t));
  return n;
}

void setup()
{
  Serial.begin(115200);
  Serial.print("Initializing SD card...");
  if (!SD.begin(33, 14, 12, 27)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");
  delay(1000);
  /* open wav file and process it */
  root = SD.open("T.WAV");
  if (root) {    
    int c = 0;
    int n;
    while (root.available()) {
      switch(state){
        case HEADER_RIFF:
        wavRiff_t wavRiff;
        n = readRiff(root, &wavRiff);
        if(n == sizeof(wavRiff_t)){
          if(wavRiff.chunkID == CCCC('R', 'I', 'F', 'F') && wavRiff.format == CCCC('W', 'A', 'V', 'E')){
            state = HEADER_FMT;
            Serial.println("HEADER_RIFF");
          }
        }
        break;
        case HEADER_FMT:
        n = readProps(root, &wavProps);
        if(n == sizeof(wavProperties_t)){
          state = HEADER_DATA;
        }
        break;
        case HEADER_DATA:
        uint32_t chunkId, chunkSize;
        n = read4bytes(root, &chunkId);
        if(n == 4){
          if(chunkId == CCCC('d', 'a', 't', 'a')){
            Serial.println("HEADER_DATA");
          }
        }
        n = read4bytes(root, &chunkSize);
        if(n == 4){
          Serial.println("prepare data");
          state = DATA;
        }
        //initialize i2s with configurations above
        i2s_driver_install((i2s_port_t)i2s_num, &i2s_config, 0, NULL);
        i2s_set_pin((i2s_port_t)i2s_num, &pin_config);
        //set sample rates of i2s to sample rate of wav file
        i2s_set_sample_rates((i2s_port_t)i2s_num, wavProps.sampleRate); 
        break; 
        /* after processing wav file, it is time to process music data */
        case DATA:
        uint32_t data; 
        n = read4bytes(root, &data);
        i2s_write_sample_nb(data); 
        break;
      }
    }
    root.close();
  } else {
    Serial.println("error opening test.txt");
  }
  i2s_driver_uninstall((i2s_port_t)i2s_num); //stop & destroy i2s driver 
  Serial.println("done!");
}

void loop()
{
}
