/*
 * Copyright (C) 2015-2016 Thomas <tomas123 @ EEVblog Electronics Community Forum>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libusb.h>
#include <unistd.h>
#include <time.h>
 
#include <fcntl.h>
#include <math.h>

#include "jpeglib.h"

#include "plank.h"

// -- define v4l2 ---------------
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#define SWITCH_DEFAULT 0
#define SWITCH_DONT_WAIT_DEVCE 1
#define SWITCH_NO_OVERLAYS 2

#define VIDEO_DEVICE0 "/dev/video1"  // gray scale thermal image
#define FRAME_WIDTH0  160
#define FRAME_HALF_WIDTH0  80
#define FRAME_HEIGHT0 120
#define FRAME_HALF_HEIGHT0 60


#define VIDEO_DEVICE1 "/dev/video2" // color visible image
#define FRAME_WIDTH1  640
#define FRAME_HEIGHT1 480

#define VIDEO_DEVICE2 "/dev/video3" // colorized thermal image
#define FRAME_WIDTH2  160
#define FRAME_HEIGHT2 128

#define FRAME_FORMAT0 V4L2_PIX_FMT_RGB24;
#define FRAME_FORMAT1 V4L2_PIX_FMT_MJPEG
#define FRAME_FORMAT2 V4L2_PIX_FMT_RGB24

struct v4l2_capability vid_caps0;
struct v4l2_capability vid_caps1;
struct v4l2_capability vid_caps2;

struct v4l2_format vid_format0;
struct v4l2_format vid_format1;
struct v4l2_format vid_format2;

size_t framesize0;
size_t linewidth0;

size_t framesize1;
size_t linewidth1;

size_t framesize2;
size_t linewidth2;

     
const char *video_device0 = VIDEO_DEVICE0;
const char *video_device1 = VIDEO_DEVICE1;
const char *video_device2 = VIDEO_DEVICE2;

int fdwr0 = 0;
int fdwr1 = 0;
int fdwr2 = 0;

// -- end define v4l2 ---------------

 #define VENDOR_ID 0x09cb
 #define PRODUCT_ID 0x1996

 static struct libusb_device_handle *devh = NULL;
 int filecount=0;
 struct timeval t1, t2;
 long long fps_t;
 
 int FFC =   0; // detect FFC

// -- buffer for EP 0x85 chunks ---------------
 #define BUF85SIZE 1048576  // size got from android app
 int buf85pointer = 0;
 unsigned char buf85[BUF85SIZE];
  
void print_format(struct v4l2_format*vid_format) 
{
  printf("     vid_format->type                =%d\n",     vid_format->type );
  printf("     vid_format->fmt.pix.width       =%d\n",     vid_format->fmt.pix.width );
  printf("     vid_format->fmt.pix.height      =%d\n",     vid_format->fmt.pix.height );
  printf("     vid_format->fmt.pix.pixelformat =%d\n",     vid_format->fmt.pix.pixelformat);
  printf("     vid_format->fmt.pix.sizeimage   =%u\n",     vid_format->fmt.pix.sizeimage );
  printf("     vid_format->fmt.pix.field       =%d\n",     vid_format->fmt.pix.field );
  printf("     vid_format->fmt.pix.bytesperline=%d\n",     vid_format->fmt.pix.bytesperline );
  printf("     vid_format->fmt.pix.colorspace  =%d\n",     vid_format->fmt.pix.colorspace );
}

//#include "font.h" 
#include "font5x7.h" 
void font_write(unsigned char *fb, int x, int y, const char *string)
{
  int rx, ry;
  while (*string) {
    for (ry = 0; ry < 5; ++ry) {
      for (rx = 0; rx < 7; ++rx) {
        int v = (font5x7_basic[((*string) & 0x7F) - CHAR_OFFSET][ry] >> (rx)) & 1;
//	    fb[(y+ry) * FRAME_WIDTH0 + (x + rx)] = v ? 0 : 0xFF;                         // black / white
//	    fb[(y+rx) * FRAME_WIDTH0 + (x + ry)] = v ? 0 : 0xFF;                         // black / white
        fb[(y+rx) * FRAME_WIDTH0 + (x + ry)] = v ? 0 : fb[(y+rx) * FRAME_WIDTH0 + (x + ry)];  // transparent
      }
    }
    string++;
    x += 6;
  }
}

double raw2temperature(unsigned short RAW)
{
  // mystery correction factor
  RAW *= 4;
  // calc amount of radiance of reflected objects ( Emissivity < 1 )
  double RAWrefl = PlanckR1 / (PlanckR2 * (exp(PlanckB / (TempReflected + 273.15)) - PlanckF))- PlanckO;
  // get displayed object temp max/min
  double RAWobj = (RAW - (1 - Emissivity) * RAWrefl) / Emissivity;
  // calc object temperature
  return PlanckB / log(PlanckR1 / (PlanckR2 * (RAWobj + PlanckO)) + PlanckF) - 273.15;  
}

void startv4l2()
{
  int ret_code = 0;

  int i;
  int k=1;

  //open video_device0
  printf("using output device: %s\n", video_device0);
  
  fdwr0 = open(video_device0, O_RDWR);
  assert(fdwr0 >= 0);

  ret_code = ioctl(fdwr0, VIDIOC_QUERYCAP, &vid_caps0);
  assert(ret_code != -1);

  memset(&vid_format0, 0, sizeof(vid_format0));

  ret_code = ioctl(fdwr0, VIDIOC_G_FMT, &vid_format0);

  linewidth0 = FRAME_WIDTH0;
  framesize0 = FRAME_WIDTH0 * FRAME_HEIGHT0 * 3; // 16 bit 
  vid_format0.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  vid_format0.fmt.pix.width = FRAME_WIDTH0;
  vid_format0.fmt.pix.height = FRAME_HEIGHT0;
  vid_format0.fmt.pix.pixelformat = FRAME_FORMAT0;
  vid_format0.fmt.pix.sizeimage = framesize0;
  vid_format0.fmt.pix.field = V4L2_FIELD_NONE;
  vid_format0.fmt.pix.bytesperline = linewidth0;
  vid_format0.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;

  // set data format
  ret_code = ioctl(fdwr0, VIDIOC_S_FMT, &vid_format0);
  assert(ret_code != -1);

  print_format(&vid_format0);


  //open video_device1
  printf("using output device: %s\n", video_device1);
  
  fdwr1 = open(video_device1, O_RDWR);
  assert(fdwr1 >= 0);

  ret_code = ioctl(fdwr1, VIDIOC_QUERYCAP, &vid_caps1);
  assert(ret_code != -1);

  memset(&vid_format1, 0, sizeof(vid_format1));

  ret_code = ioctl(fdwr1, VIDIOC_G_FMT, &vid_format1);

  linewidth1 = FRAME_WIDTH1;
  framesize1 = FRAME_WIDTH1 * FRAME_HEIGHT1 * 1; // 8 Bit ??

  vid_format1.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  vid_format1.fmt.pix.width = FRAME_WIDTH1;
  vid_format1.fmt.pix.height = FRAME_HEIGHT1;
  vid_format1.fmt.pix.pixelformat = FRAME_FORMAT1;
  vid_format1.fmt.pix.sizeimage = framesize1;
  vid_format1.fmt.pix.field = V4L2_FIELD_NONE;
  vid_format1.fmt.pix.bytesperline = linewidth1;
  vid_format1.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;

  // set data format
  ret_code = ioctl(fdwr1, VIDIOC_S_FMT, &vid_format1);
  assert(ret_code != -1);

  print_format(&vid_format1);


  //open video_device2
  printf("using output device: %s\n", video_device2);
  
  fdwr2 = open(video_device2, O_RDWR);
  assert(fdwr2 >= 0);

  ret_code = ioctl(fdwr2, VIDIOC_QUERYCAP, &vid_caps2);
  assert(ret_code != -1);

  memset(&vid_format2, 0, sizeof(vid_format2));

  ret_code = ioctl(fdwr2, VIDIOC_G_FMT, &vid_format2);

  linewidth2 =FRAME_WIDTH2;
  framesize2 = FRAME_WIDTH2 * FRAME_HEIGHT2 * 3; // 8x8x8 Bit

  vid_format2.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  vid_format2.fmt.pix.width = FRAME_WIDTH2;
  vid_format2.fmt.pix.height = FRAME_HEIGHT2;
  vid_format2.fmt.pix.pixelformat = FRAME_FORMAT2;
  vid_format2.fmt.pix.sizeimage = framesize2;
  vid_format2.fmt.pix.field = V4L2_FIELD_NONE;
  vid_format2.fmt.pix.bytesperline = linewidth2;
  vid_format2.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;

  // set data format
  ret_code = ioctl(fdwr2, VIDIOC_S_FMT, &vid_format2);
  assert(ret_code != -1);

  print_format(&vid_format2);
}

// unused
void closev4l2()
{
  close(fdwr0);
  close(fdwr1);
  close(fdwr2);

}

void buffer_reshape(unsigned short* pix)
{
  // Make a unsigned short array from what comes from the thermal frame
  int v;
  for (int y = 0; y < FRAME_HEIGHT0; ++y) 
  {
    for (int x = 0; x < FRAME_WIDTH0; ++x) {
      if (x < 80) 
         v = buf85[2 * (y * 164 + x) + 32]     + 256 * buf85[2 * (y * 164 + x) + 33];
      else
         v = buf85[2 * (y * 164 + x) + 32 + 4] + 256 * buf85[2 * (y * 164 + x) + 33 + 4];   
      
      pix[y * FRAME_WIDTH0 + x] = v;   // unsigned char!!
    }
  }
}

void get_extreme_values(unsigned short* pix, int *min, int *max, int* maxx, int* maxy, float* rms)
{
  *min = 0x10000;
  *max = 0;
  int v;
  for (int y = 0; y < FRAME_HEIGHT0; ++y) 
  {
    for (int x = 0; x < FRAME_WIDTH0; ++x) {
      v = pix[y * FRAME_WIDTH0 + x];
      
      if (v < *min) { *min = v; }
      if (v > *max) { *max = v; *maxx = x; *maxy = y; }
      *rms += v * v;
    }
  }
  // RMS used later
  // rms /= FRAME_WIDTH0 * FRAME_HEIGHT0;
  // rms = sqrtf(rms);
}

void scale_data(int min, int max, unsigned short* pix, unsigned char* fb_proc)
{
  // scale the data in the array pix to array fb_proc
  int delta = max - min;
  if (!delta) delta = 1;   // if max = min we have divide by zero
  int scale = 0x10000 / delta;

  for (int y = 0; y < FRAME_HEIGHT0; ++y)
  {
    for (int x = 0; x < FRAME_WIDTH0; ++x)
    {
      int v = (pix[y * FRAME_WIDTH0 + x] - min) * scale >> 8;

      // fb_proc is the gray scale frame buffer
      fb_proc[y * FRAME_WIDTH0 + x] = v;   // unsigned char!!

    }
  }
}

void overlays_write(int min, int max, int maxx, int maxy, unsigned short* pix, unsigned char* fb_proc)
{
  char st1[100];
  char st2[100];

  time_t now1;
  struct tm *loctime;
  // Convert it to local time and Print it out in a nice format.
  loctime = localtime (&now1);
  strftime (st1, 60, "%H:%M:%S", loctime);
   
  // calc medium of 2x2 center pixels
  int med = (pix[(FRAME_HALF_HEIGHT0 - 1) * FRAME_WIDTH0 + FRAME_HALF_WIDTH0 - 1] + pix[(FRAME_HALF_HEIGHT0 - 1) * FRAME_WIDTH0 + FRAME_HALF_WIDTH0] + 
             pix[FRAME_HALF_HEIGHT0       * FRAME_WIDTH0 + FRAME_HALF_WIDTH0 - 1] + pix[FRAME_HALF_HEIGHT0       * FRAME_WIDTH0 + FRAME_HALF_WIDTH0]) / 4;
  sprintf(st2," %.1f/%.1f/%.1f'C", raw2temperature(min), raw2temperature(med), raw2temperature(max));
  strcat(st1, st2);
  
  #define MAX 26 // max chars in line  160/6=26,6 
  strncpy(st2, st1, MAX);
  // write zero to string !! 
  st2[MAX-1] = '\0';
  font_write(fb_proc, 1, FRAME_HEIGHT0, st2);

  // show crosshairs, remove if required 
  font_write(fb_proc, FRAME_HALF_WIDTH0 - 2, FRAME_HALF_HEIGHT0 - 3, "+");

  maxx -= 4;
  maxy -= 4;

  if (maxx < 0) maxx = 0; 
  if (maxy < 0) maxy = 0;
  if (maxx > FRAME_WIDTH0  - 10) maxx = FRAME_WIDTH0  - 10;
  if (maxy > FRAME_HEIGHT0 - 10) maxy = FRAME_HEIGHT0 - 10;

  font_write(fb_proc, FRAME_WIDTH2 - 6, maxy, "<");
  font_write(fb_proc, maxx, FRAME_HEIGHT0 - 8, "|");
}

void palette_apply(unsigned char *colormap, int frame_height, unsigned char* fb_proc, unsigned char* fb_proc2)
{
  for (int y = 0; y < frame_height; ++y) 
  {
    for (int x = 0; x < FRAME_WIDTH2; ++x) 
    {  
      // fb_proc is the gray scale frame buffer
      int v = fb_proc[y * FRAME_WIDTH2 + x] ;

      // fb_proc2 is an 24bit RGB buffer
      fb_proc2[ 3 * y * FRAME_WIDTH2 + x * 3]      = colormap[3 * v]; 
      fb_proc2[(3 * y * FRAME_WIDTH2 + x * 3) + 1] = colormap[3 * v + 1];
      fb_proc2[(3 * y * FRAME_WIDTH2 + x * 3) + 2] = colormap[3 * v + 2];
    }
  }
}

void transfer_raw(int unsigned short* pix, unsigned char* fb_proc0)
{
  // transfer 16 bit raw grayscale
  for (int y = 0; y < FRAME_HEIGHT0; ++y)
  {
    for (int x = 0; x < FRAME_WIDTH0; ++x)
    {
      int v = pix[y * FRAME_WIDTH0 + x];

      unsigned char r = 0;
      unsigned char g = (unsigned char) ((v >> 8) & 0xff);
      unsigned char b = (unsigned char) (v & 0xff);

      fb_proc0[ 3 * y * FRAME_WIDTH0 + x * 3]       = r;
      fb_proc0[(3 * y * FRAME_WIDTH0 + x * 3) + 1] = g;
      fb_proc0[(3 * y * FRAME_WIDTH0 + x * 3) + 2] = b;
    }
  }
}

void vframe(char ep[],char EP_error[], int r, int actual_length, unsigned char buf[], unsigned char *colormap, unsigned char switches) 
{
  // error handler
  time_t now1;
  now1 = time(NULL); 
  if (r < 0) {
    if (strcmp (EP_error, libusb_error_name(r)) !=0 )
    {       
      strcpy(EP_error, libusb_error_name(r));
      fprintf(stderr, "\n: %s >>>>>>>>>>>>>>>>>bulk transfer (in) %s:%i %s\n", ctime(&now1), ep , r, libusb_error_name(r));
      sleep(1);
    }
    return;
  }
  
  // reset buffer if the new chunk begins with magic bytes or the buffer size limit is exceeded
  unsigned char magicbyte[4]={0xEF, 0xBE, 0x00, 0x00};
  
  if  ((strncmp (buf, magicbyte, 4) == 0 ) || ((buf85pointer + actual_length) >= BUF85SIZE))
  {
    //printf(">>>>>>>>>>>begin of new frame<<<<<<<<<<<<<\n");
    buf85pointer = 0;
  }
 
  //printf("actual_length %d !!!!!\n", actual_length);

  memmove(buf85 + buf85pointer, buf, actual_length);
  buf85pointer = buf85pointer + actual_length;
  
  if  ((strncmp(buf85, magicbyte, 4) != 0 ))
    {
        //reset buff pointer
        buf85pointer=0;
        printf("Reset buffer because of bad Magic Byte!\n");
        return;
    }
      
  // a quick and dirty job for gcc
  uint32_t FrameSize   = buf85[ 8] + (buf85[ 9] << 8) + (buf85[10] << 16) + (buf85[11] << 24);
  uint32_t ThermalSize = buf85[12] + (buf85[13] << 8) + (buf85[14] << 16) + (buf85[15] << 24);
  uint32_t JpgSize     = buf85[16] + (buf85[17] << 8) + (buf85[18] << 16) + (buf85[19] << 24);
  uint32_t StatusSize  = buf85[20] + (buf85[21] << 8) + (buf85[22] << 16) + (buf85[23] << 24);

  //printf("FrameSize= %d (+28=%d), ThermalSize %d, JPG %d, StatusSize %d, Pointer %d\n",FrameSize,FrameSize+28, ThermalSize, JpgSize,StatusSize,buf85pointer); 

  if ( (FrameSize + 28) > (buf85pointer) ) 
  {
    // wait for next chunk
    return;
  }
  
  // get a full frame, first print the status
  t1 = t2;
  gettimeofday(&t2, NULL);
// fps as moving average over last 20 frames
// ps_t = (19*fps_t+10000000/(((t2.tv_sec * 1000000) + t2.tv_usec) - ((t1.tv_sec * 1000000) + t1.tv_usec)))/20;

  filecount++;
//  printf("#%08i %lld/10 fps:",filecount,fps_t); 
//  for (int i = 0; i < StatusSize; i++) 
//  {
//    int v = 28 + ThermalSize+JpgSize + i;
//    if (buf85[v] > 31) { printf("%c", buf85[v]); }
//  }
//  printf("\n"); 
  
  buf85pointer = 0;
  
  unsigned short pix[FRAME_WIDTH0 * FRAME_HEIGHT0];   // original Flir 16 Bit RAW
  int min, max, maxx, maxy;
  float rms = 0;
  buffer_reshape(&pix[0]);
  get_extreme_values(&pix[0], &min, &max, &maxx, &maxy, &rms);

  unsigned char *fb_proc0, *fb_proc, *fb_proc2;

  fb_proc0 = malloc(FRAME_WIDTH0 * FRAME_HEIGHT0 * 3);   // 8x8x8 bit RGB buffer for RAW data thermal image

  int frame_height;
  if (switches & SWITCH_NO_OVERLAYS)
  {
    frame_height = FRAME_HEIGHT0;
  }
  else
  {
    frame_height = FRAME_HEIGHT2;
  }
  
  fb_proc  = malloc(FRAME_WIDTH2 * frame_height);       // 8 Bit gray buffer
  memset(fb_proc, 128, FRAME_WIDTH2 * frame_height);    // sizeof(fb_proc) doesn't work, value depends from LUT
  fb_proc2 = malloc(FRAME_WIDTH2 * frame_height * 3);   // 8x8x8 bit RGB buffer
  
  transfer_raw(&pix[0], &fb_proc0[0]);
  scale_data(min, max, pix, fb_proc);
  if (!(switches & SWITCH_NO_OVERLAYS)) overlays_write(min, max, maxx, maxy, &pix[0], &fb_proc[0]);
  palette_apply(colormap, frame_height, fb_proc, fb_proc2);
    
  //write video to v4l2loopback(s)
  write(fdwr1, &buf85[28 + ThermalSize], JpgSize);  // jpg Visual Image
  if (strncmp (&buf85[28 + ThermalSize + JpgSize + 17], "FFC", 3) == 0)
  {
    FFC = 1;  // drop all FFC frames
  } 
  else    
  {        
    if (FFC == 1)
    {
      FFC = 0; // drop first frame after FFC
      //printf("FFC frame\n");
      
    }
    else
    {             
      write(fdwr0, fb_proc0, framesize0);  // gray scale Thermal Image
      write(fdwr2, fb_proc2, framesize2);  // colorized RGB Thermal Image
    }
  }

  // free memory
  free(fb_proc0);                   // thermal RAW
  free(fb_proc);                    // grayscale scaled thermal
  free(fb_proc2);                   // visible jpg
    
}

static int find_lvr_flirusb(void)
{
 	devh = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, PRODUCT_ID);
 	return devh ? 0 : -EIO;
}
 
void print_bulk_result(char ep[],char EP_error[], int r, int actual_length, unsigned char buf[])
{
  time_t now1;
  int i;

  now1 = time(NULL);
  if (r < 0)
  {
    if (strcmp (EP_error, libusb_error_name(r))!=0)
    {       
      strcpy(EP_error, libusb_error_name(r));
      fprintf(stderr, "\n: %s >>>>>>>>>>>>>>>>>bulk transfer (in) %s:%i %s\n", ctime(&now1), ep , r, libusb_error_name(r));
      sleep(1);
    }
    //return 1;
  } 
  else
  {           
    printf("\n: %s bulk read EP %s, actual length %d\nHEX:\n",ctime(&now1), ep ,actual_length);
    
    // write frame to file          
    /*
    char filename[100];
    sprintf(filename, "EP%s#%05i.bin",ep,filecount);
    filecount++;
    FILE *file = fopen(filename, "wb");
    fwrite(buf, 1, actual_length, file);
    fclose(file);
    */

    // hex print of first byte
    for (i = 0; i <  (((200)<(actual_length))?(200):(actual_length)); i++)
    {
      printf(" %02x", buf[i]);
    }
          
    printf("\nSTRING:\n");	
    for (i = 0; i <  (((200)<(actual_length))?(200):(actual_length)); i++) 
    {
      if(buf[i]>31) {printf("%c", buf[i]);}
    }
    printf("\n");	
      
  } 
}
 
int EPloop(unsigned char *colormap, unsigned short switches)
{ 
  static int is_error = FALSE; 

  int i, r = 1;

 	r = libusb_init(NULL);
 	if (r < 0)
  {
 		if (!is_error) fprintf(stderr, "failed to initialise libusb\n");
 		exit(1);
 	}
 	
  r = find_lvr_flirusb();
 	if (r < 0) 
  {
 		if (!is_error) fprintf(stderr, "Could not find/open device\n");
 		goto out;
 	}
 	printf("Successfully find the Flir One G2 device\n");
	

  r = libusb_set_configuration(devh, 3);
  if (r < 0)
  {
      if (!is_error) fprintf(stderr, "libusb_set_configuration error %d\n", r);
      goto out;
  }
  printf("Successfully set usb configuration 3\n");
	
 
 	// Claiming of interfaces is a purely logical operation; 
  // it does not cause any requests to be sent over the bus. 
 	r = libusb_claim_interface(devh, 0);
 	if (r < 0) 
  {
 		if (!is_error) fprintf(stderr, "libusb_claim_interface 0 error %d\n", r);
 		goto out;
 	}	

 	r = libusb_claim_interface(devh, 1);
 	if (r < 0) 
  {
 		if (!is_error) fprintf(stderr, "libusb_claim_interface 1 error %d\n", r);
 		goto out;
 	}

 	r = libusb_claim_interface(devh, 2);
 	if (r < 0)
  {
 		if (!is_error) fprintf(stderr, "libusb_claim_interface 2 error %d\n", r);
 		goto out;
 	}

  is_error = FALSE;

 	printf("Successfully claimed interface 0,1,2\n");
 	
	unsigned char buf[1048576]; 
  int actual_length;

 	time_t now;
 	// save last error status to avoid clutter the log
	char EP81_error[50] = "", EP83_error[50] = "", EP85_error[50] = ""; 
 	unsigned char data[2] = {0, 0}; // only a bad dummy
 	
 	// don't forget: $ sudo modprobe v4l2loopback video_nr=0,1
 	startv4l2();
 	
 	int state = 1; 
 	int ct=0;

  while (1)
  {
    
    switch(state)
    {
        
        case 1:
          /*
          
          Flir config
          01 0b 01 00 01 00 00 00 c4 d5
          0 bmRequestType = 01
          1 bRequest = 0b
          2 wValue 0001 type (H) index (L)    stop=0/start=1 (Alternate Setting)
          4 wIndex 01                         interface 1/2
          5 wLength 00
          6 Data 00 00

          libusb_control_transfer (*dev_handle, bmRequestType, bRequest, wValue,  wIndex, *data, wLength, timeout)

          */

          printf("stop interface 2 FRAME\n");
          r = libusb_control_transfer(devh, 1, 0x0b, 0, 2, data, 0, 100);
          if (r < 0) {
              fprintf(stderr, "Control Out error %d\n", r);
              return r;
          }

          printf("stop interface 1 FILEIO\n");
          r = libusb_control_transfer(devh, 1, 0x0b, 0, 1, data, 0, 00);
          if (r < 0) {
              fprintf(stderr, "Control Out error %d\n", r);
              return r;
          } 
            
          printf("\nstart interface 1 FILEIO\n");
          r = libusb_control_transfer(devh, 1, 0x0b, 1, 1, data, 0, 100);
          if (r < 0) {
            fprintf(stderr, "Control Out error %d\n", r);
            return r;
          }
          now = time(0); // Get the system time
          printf("\n:xx %s", ctime(&now));
          state = 3;   // jump over wait stait 2. Not really using any data from CameraFiles.zip
          break;
        
        
        case 2:
          printf("\nask for CameraFiles.zip on EP 0x83:\n");     
          now = time(0); // Get the system time
          printf("\n: %s",ctime(&now));
    
          int transferred = 0;
          char my_string[128];

          //--------- write string: {"type":"openFile","data":{"mode":"r","path":"CameraFiles.zip"}}
          int length = 16;
          unsigned char my_string2[16]={0xcc,0x01,0x00,0x00,0x01,0x00,0x00,0x00,0x41,0x00,0x00,0x00,0xF8,0xB3,0xF7,0x00};
          printf("\nEP 0x02 to be sent Hexcode: %i Bytes[",length);
          int i;
          for (i = 0; i < length; i++)
          {
              printf(" %02x", my_string2[i]);
          }
          printf(" ]\n");
    
          r = libusb_bulk_transfer(devh, 2, my_string2, length, &transferred, 0);
          if(r == 0 && transferred == length)
          {
            printf("\nWrite successful!");
          }
          else
          {
            printf("\nError in write! res = %d and transferred = %d\n", r, transferred);
          }
    
          strcpy(  my_string,"{\"type\":\"openFile\",\"data\":{\"mode\":\"r\",\"path\":\"CameraFiles.zip\"}}");
    
          length = strlen(my_string)+1;
          printf("\nEP 0x02 to be sent: %s", my_string);
    
          // avoid error: invalid conversion from ‘char*’ to ‘unsigned char*’ [-fpermissive]
          unsigned char *my_string1 = (unsigned char*)my_string;
          //my_string1 = (unsigned char*)my_string;
            
          r = libusb_bulk_transfer(devh, 2, my_string1, length, &transferred, 0);
          if(r == 0 && transferred == length)
          {
            printf("\nWrite successful!");
            printf("\nSent %d bytes with string: %s\n", transferred, my_string);
          }
          else
          {
            printf("\nError in write! res = %d and transferred = %d\n", r, transferred);
          }

          //--------- write string: {"type":"readFile","data":{"streamIdentifier":10}}
          length = 16;
          unsigned char my_string3[16]={0xcc,0x01,0x00,0x00,0x01,0x00,0x00,0x00,0x33,0x00,0x00,0x00,0xef,0xdb,0xc1,0xc1};
          printf("\nEP 0x02 to be sent Hexcode: %i Bytes[",length);
          for (i = 0; i < length; i++) 
          {
            printf(" %02x", my_string3[i]);
          }
          printf(" ]\n");
  
          r = libusb_bulk_transfer(devh, 2, my_string3, length, &transferred, 0);
          if(r == 0 && transferred == length)
          {
            printf("\nWrite successful!");
          }
          else
          {
            printf("\nError in write! res = %d and transferred = %d\n", r, transferred);
          }

          //strcpy(  my_string, "{\"type\":\"setOption\",\"data\":{\"option\":\"autoFFC\",\"value\":true}}");
          strcpy( my_string, "{\"type\":\"readFile\",\"data\":{\"streamIdentifier\":10}}");
          length = strlen(my_string) + 1;
          printf("\nEP 0x02 to be sent %i Bytes: %s", length, my_string);
  
          // avoid error: invalid conversion from ‘char*’ to ‘unsigned char*’ [-fpermissive]
          my_string1 = (unsigned char*)my_string;
          
          r = libusb_bulk_transfer(devh, 2, my_string1, length, &transferred, 0);
          if(r == 0 && transferred == length)
          {
              printf("\nWrite successful!");
              printf("\nSent %d bytes with string: %s\n", transferred, my_string);
          }
          else
              printf("\nError in write! res = %d and transferred = %d\n", r, transferred);


          // go to next state
          now = time(0); // Get the system time
          printf("\n: %s", ctime(&now));
          //sleep(1);
          state = 3;           
          break;
    

        case 3:
          printf("\nAsk for video stream, start EP 0x85:\n");        

          r = libusb_control_transfer(devh,1, 0x0b, 1, 2, data, 2, 200);
          if (r < 0) 
          {
              fprintf(stderr, "Control Out error %d\n", r);
              return r;
          };

          state = 4;
          break;

        case 4:
          // endless loop 
          // poll Frame Endpoints 0x85 
          // don't change timeout=100ms !!
          r = libusb_bulk_transfer(devh, 0x85, buf, sizeof(buf), &actual_length, 100); 
          if (actual_length > 0)
          {
            vframe("0x85", EP85_error, r, actual_length, buf, colormap, switches);
          }
  
          break;      

        }    

      // poll Endpoints 0x81, 0x83
      r = libusb_bulk_transfer(devh, 0x81, buf, sizeof(buf), &actual_length, 10); 
      
      /*

      if (actual_length > 0 && actual_length <= 101)
      {
        char k[5];
        if (strncmp (&buf[32],"VoltageUpdate",13)==0)
        {
          printf("xx %d\n",actual_length);


          char *token, *string, *tofree, *string2;
//      	char l;
          strcpy(string,buf);
//        string = buf;
//	      assert(string != NULL);
          printf("yy\n");

          for (i = 32; i <  (((200)<(actual_length))?(200):(actual_length)); i++) 
          {
            if(string[i]>31) 
            {
              printf("%c", string[i]);
//	      	  printf("%d ", i);
//	      	  string2[i-32] = string[i];
            }
          }

          while ((token = strsep(&string, ":")) != NULL)
          {      
            printf("zz\n");
            printf("%s\n", token);
          }

//        free(tofree);
//        for (i = 32; i <  (((200)<(actual_length))?(200):(actual_length)); i++) 
//        {
//          if(buf[i]>31) 
//          {
//            printf("%c", buf[i]);
//          }
//        }
        }
      }

*/

      r = libusb_bulk_transfer(devh, 0x83, buf, sizeof(buf), &actual_length, 10); 
      if (strcmp(libusb_error_name(r), "LIBUSB_ERROR_NO_DEVICE")==0) 
      {
        fprintf(stderr, "EP 0x83 LIBUSB_ERROR_NO_DEVICE -> reset USB\n");
        goto out;
      } 
//    print_bulk_result("0x83",EP83_error, r, actual_length, buf); 
  
  }
    
  // never reached ;-)
 	libusb_release_interface(devh, 0);
 	
  out:

    if (!is_error && !(switches & SWITCH_DONT_WAIT_DEVCE))
    {
      fprintf(stderr, "Waiting for device...\n");
    }

    is_error = TRUE;
    

  //close the device
  
  if (devh != NULL)
  {
 	  libusb_reset_device(devh);
  }
  libusb_close(devh);
 	libusb_exit(NULL);

  if (switches & SWITCH_DONT_WAIT_DEVCE) 
  {
    exit(1);
  }

 	return r >= 0 ? r : -r;
}

void usage_print(char* arg)
{
	fprintf(stderr, "Usage: %s [--waitdevice] [--nooverlays] palette.raw\n", arg) ;
}

unsigned short parse_args(int argc, char **argv)
{
  unsigned short switches = 0x00;
  char* switch_prefix = "--";
  char* palette_file;

  if(argc < 2) 
  {
    fprintf(stderr, "\n");
		usage_print(argv[0]);
    exit(1);
	}

  if  (argc == 2)
  {
    if (strncmp(argv[1], switch_prefix, 2) == 0)
    {
      fprintf(stderr, "\nPalette file absent.\n");
      usage_print(argv[0]);
      exit(1);
    }
    else
    {
      palette_file = argv[1];
    }  
  }

  if (argc > 2)
  {
    for(int i=1; i<argc; i++)
    {
      // switches
      if (strncmp(argv[i], "--dontwaitdevice", 17) == 0)
      {
        switches |= SWITCH_DONT_WAIT_DEVCE;
      }
      else if (strncmp(argv[i], "--nooverlays", 13) == 0)
      {
        switches |= SWITCH_NO_OVERLAYS;
      }
      else if (strncmp(argv[i], switch_prefix, strlen(switch_prefix)) == 0)
      {
        fprintf(stderr, "\nUnknown switch '%s'.\n", argv[i]);
        usage_print(argv[0]);
        exit(1);
      }
      else if (i != (argc - 1))
      {
        fprintf(stderr, "\nParameter not recognized: '%s'. Pallete file must be the last parameter.\n", argv[i]);
        usage_print(argv[0]);
        exit(1);
      } 
    }
    palette_file = argv[argc - 1];
  }
  return switches;
}

unsigned char* read_palette(char* palette_file)
{
  static unsigned char colormap[768];
  FILE *fp;
  fp = fopen(palette_file, "rb");
  if (fp != NULL)
  {
    fread(colormap, sizeof(unsigned char), 768, fp);  // read 256 rgb values
    fclose(fp);
  }
  else
  {
     fprintf(stderr, "\nPalette file '%s' not found.\n", palette_file);
     exit(1);
  }
  return colormap;
}

int main(int argc, char **argv)
{
	unsigned short switches = parse_args(argc, argv);
  char* palette_file = argv[argc - 1];
  
  unsigned char* colormap = read_palette(palette_file);
   
  while (1)
  {
    EPloop(colormap, switches);
  }
  
} 
