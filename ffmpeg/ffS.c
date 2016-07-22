#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/frame.h>
#include <pthread.h>
#include <SDL.h>
#include <SDL_thread.h>

#include <stdio.h>

#ifdef __MINGW32__
#undef main /* Prevents SDL from overriding main() */
#endif

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc  avcodec_alloc_frame
#endif

typedef unsigned char uchar;

#define MAX_CACHE 10
AVFrame* videoCache[MAX_CACHE];
int numCacheFrame = 0;
int videoCacheIdx = 0;

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame);

 int loadVideo2Memory(char* filename) {
 // Initalizing these to NULL prevents segfaults!
 AVFormatContext *pFormatCtx = NULL;
 int i, videoStream;
 AVCodecContext *pCodecCtxOrig = NULL;
 AVCodecContext *pCodecCtx = NULL;
 AVCodec *pCodec = NULL;
 AVFrame *pFrame = NULL;
 AVFrame *pFrameYUV = NULL;

 AVPacket packet;
 int frameFinished;
 int numBytesYUV;
 uint8_t *bufferYUV = NULL;
 struct SwsContext *sws_ctxYUV = NULL;

 // Register all formats and codecs
 av_register_all();

 // Open video file
 if (avformat_open_input(&pFormatCtx, filename, NULL, NULL) != 0)
 return -1; // Couldn't open file

 // Retrieve stream information
 if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
 return -1; // Couldn't find stream information

 // Dump information about file onto standard error
 av_dump_format(pFormatCtx, 0, filename, 0);

 // Find the first video stream
 videoStream = -1;
 for (i = 0; i < pFormatCtx->nb_streams; i++)
 if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
 videoStream = i;
 break;
 }
 if (videoStream == -1)
 return -1; // Didn't find a video stream

 // Get a pointer to the codec context for the video stream
 pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;
 // Find the decoder for the video stream
 pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
 if (pCodec == NULL) {
 fprintf(stderr, "Unsupported codec!\n");
 return -1; // Codec not found
 }
 // Copy context
 pCodecCtx = avcodec_alloc_context3(pCodec);
 if (avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
 fprintf(stderr, "Couldn't copy codec context");
 return -1; // Error copying codec context
 }

 // Open codec
 if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
 return -1; // Could not open codec
if(pCodecCtx->pix_fmt != PIX_FMT_YUV422P){
	printf("Error in pixel format \n");
	return -1;
}
 // Allocate video frame
 pFrame = av_frame_alloc();
 pFrameYUV = av_frame_alloc();

 // content to convert pixel format
 sws_ctxYUV = sws_getContext(pCodecCtx->width, pCodecCtx->height,
 pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
 PIX_FMT_YUV422P, SWS_BILINEAR, NULL, NULL, NULL);

 // Determine required buffer size and allocate buffer for YUV
 numBytesYUV = avpicture_get_size(PIX_FMT_YUV422P, pCodecCtx->width,
 pCodecCtx->height);
 bufferYUV = (uint8_t *) av_malloc(numBytesYUV * sizeof(uint8_t));
 // Assign appropriate parts of buffer to image planes in pFrameYUV
 // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
 // of AVPicture
 avpicture_fill((AVPicture *) pFrameYUV, bufferYUV, PIX_FMT_YUV422P,
 pCodecCtx->width, pCodecCtx->height);

 // Read frames
 i = 0;
 while (av_read_frame(pFormatCtx, &packet) >= 0) {
 // Is this a packet from the video stream?
 if (packet.stream_index == videoStream) {
 // Decode video frame
 avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

 // Did we get a video frame?
 if (frameFinished) {
 sws_scale(sws_ctxYUV, (uint8_t const * const *) pFrame->data,
 pFrame->linesize, 0, pCodecCtx->height,
 pFrameYUV->data, pFrameYUV->linesize);
 // add to videoCache variable
 AVFrame* NewFrame = av_frame_alloc();
 NewFrame->width = pCodecCtx->width;
 NewFrame->height = pCodecCtx->height;

 uint8_t* buffer = (uint8_t*) av_malloc(
 numBytesYUV * sizeof(uint8_t));

 avpicture_fill((AVPicture*) NewFrame, buffer, PIX_FMT_YUV422P,
 NewFrame->width, NewFrame->height);

 memcpy(NewFrame->data[0], pFrameYUV->data[0],
 NewFrame->linesize[0] * NewFrame->height);
 memcpy(NewFrame->data[1], pFrameYUV->data[1],
 NewFrame->linesize[1] * NewFrame->height);
 memcpy(NewFrame->data[2], pFrameYUV->data[2],
 NewFrame->linesize[2] * NewFrame->height);
 if (numCacheFrame >= MAX_CACHE) {
 printf("Meet the limit of cache size, no loading more\n");
 break;
 }
 printf("numCacheFrame: %d\n", numCacheFrame);
 videoCache[numCacheFrame] = NewFrame;
 numCacheFrame++;
 }
 }

 }

 // Close the codec
 avcodec_close(pCodecCtx);
 avcodec_close(pCodecCtxOrig);

 // Close the video file
 avformat_close_input(&pFormatCtx);

 // Free YUV422p frame
 av_free(bufferYUV);
 av_frame_free(&pFrameYUV);
 // Free the YUV frame
 av_frame_free(&pFrame);
 return 1;
 }

// LOAD VIDEO SECTION
void* loadVideo2Memory2(void* ptr) {
	char* filename;
	filename = (char*) ptr;
	printf("Load background to memory!\n");
	// Initalizing these to NULL prevents segfaults!
	AVFormatContext *pFormatCtx = NULL;
	int i, videoStream;
	AVCodecContext *pCodecCtxOrig = NULL;
	AVCodecContext *pCodecCtx = NULL;
	AVCodec *pCodec = NULL;
	AVFrame *pFrame = NULL;
	AVFrame *pFrameYUV = NULL;

	AVPacket packet;
	int frameFinished;
	int numBytesYUV;
	uint8_t *bufferYUV = NULL;
	struct SwsContext *sws_ctxYUV = NULL;

	// Register all formats and codecs
	av_register_all();

	// Open video file
	if (avformat_open_input(&pFormatCtx, filename, NULL, NULL) != 0) {
		printf("Couldn't open file! \n");
		return -1; // Couldn't open file
	}

	// Retrieve stream information
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
		return -1; // Couldn't find stream information

	// Dump information about file onto standard error
	av_dump_format(pFormatCtx, 0, filename, 0);

	// Find the first video stream
	videoStream = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}
	if (videoStream == -1)
		return -1; // Didn't find a video stream

	// Get a pointer to the codec context for the video stream
	pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;
	// Find the decoder for the video stream
	pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
	if (pCodec == NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1; // Codec not found
	}
	// Copy context
	pCodecCtx = avcodec_alloc_context3(pCodec);
	if (avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
		fprintf(stderr, "Couldn't copy codec context");
		return -1; // Error copying codec context
	}

	// Open codec
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
		return -1; // Could not open codec

	// Allocate video frame
	pFrame = av_frame_alloc();
	pFrameYUV = av_frame_alloc();

	// content to convert pixel format
	sws_ctxYUV = sws_getContext(pCodecCtx->width, pCodecCtx->height,
			pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
			PIX_FMT_YUV422P, SWS_BILINEAR, NULL, NULL, NULL);

	// Determine required buffer size and allocate buffer for YUV
	numBytesYUV = avpicture_get_size(PIX_FMT_YUV422P, pCodecCtx->width,
			pCodecCtx->height);
	bufferYUV = (uint8_t *) av_malloc(numBytesYUV * sizeof(uint8_t));
	// Assign appropriate parts of buffer to image planes in pFrameYUV
	// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
	// of AVPicture
	avpicture_fill((AVPicture *) pFrameYUV, bufferYUV, PIX_FMT_YUV422P,
			pCodecCtx->width, pCodecCtx->height);

	// Read frames
	i = 0;
	while (av_read_frame(pFormatCtx, &packet) >= 0) {
		// Is this a packet from the video stream?
		if (packet.stream_index == videoStream) {
			// Decode video frame
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

			// Did we get a video frame?
			if (frameFinished) {

				sws_scale(sws_ctxYUV, (uint8_t const * const *) pFrame->data,
						pFrame->linesize, 0, pCodecCtx->height,
						pFrameYUV->data, pFrameYUV->linesize);
				// add to videoCache variable
				AVFrame* NewFrame = av_frame_alloc();
				NewFrame->width = pCodecCtx->width;
				NewFrame->height = pCodecCtx->height;

				uint8_t* buffer = (uint8_t*) av_malloc(
						numBytesYUV * sizeof(uint8_t));

				avpicture_fill((AVPicture*) NewFrame, buffer, PIX_FMT_YUV422P,
						NewFrame->width, NewFrame->height);

				memcpy(NewFrame->data[0], pFrameYUV->data[0],
						NewFrame->linesize[0] * NewFrame->height);
				memcpy(NewFrame->data[1], pFrameYUV->data[1],
						NewFrame->linesize[1] * NewFrame->height);
				memcpy(NewFrame->data[2], pFrameYUV->data[2],
						NewFrame->linesize[2] * NewFrame->height);

				//new
				/*
				 AVFrame* NewFrame = av_frame_alloc();
				 NewFrame->width = pCodecCtx->width;
				 NewFrame->height = pCodecCtx->height;

				 uint8_t* buffer = (uint8_t*) av_malloc(
				 numBytesYUV * sizeof(uint8_t));

				 avpicture_fill((AVPicture*) NewFrame, buffer, PIX_FMT_YUV422P,
				 NewFrame->width, NewFrame->height);


				 memcpy(NewFrame->data[0], pFrame->data[0],
				 NewFrame->linesize[0] * NewFrame->height);
				 memcpy(NewFrame->data[1], pFrame->data[1],
				 NewFrame->linesize[1] * NewFrame->height);
				 memcpy(NewFrame->data[2], pFrame->data[2],
				 NewFrame->linesize[2] * NewFrame->height);
				 //new
				 */
				if (numCacheFrame >= MAX_CACHE) {
					printf("Meet the limit of cache size, no loading more\n");
					break;
				}
				//printf("numCacheFrame: %d\n", numCacheFrame);
				videoCache[numCacheFrame] = NewFrame;
				numCacheFrame++;
			}
		}

	}

	// Close the codec
	avcodec_close(pCodecCtx);
	avcodec_close(pCodecCtxOrig);

	// Close the video file
	avformat_close_input(&pFormatCtx);

	// Free YUV422p frame
	av_free(bufferYUV);
	av_frame_free(&pFrameYUV);
	// Free the YUV frame
	av_frame_free(&pFrame);

}

int freeVideoCache() {
	printf("Free memory ! \n");
	int i;
	for (i = 0; i < numCacheFrame; i++) {
		av_frame_free(&videoCache[i]);
		//av_free(videoCache[i]);
	}

	return 1;
}

int main(int argc, char *argv[]) {

	loadVideo2Memory("//media//linh//8AB81BA6B81B8FB5//pVideo//bokehBack.mov");
	return 1;
	pthread_t thread1;
	const char *message1 = "//media//linh//8AB81BA6B81B8FB5//pVideo//bokehBack.mov";
	int iret1;
	iret1 = pthread_create(&thread1, NULL, loadVideo2Memory2, (void*) message1);
	if (iret1) {
		fprintf(stderr, "Error - pthread_create() return code: %d\n", iret1);
		exit(EXIT_FAILURE);
	}
    pthread_join( thread1, NULL);

	// save 5 frame in videoCache
	AVFrame *pFrameRGB = NULL;
	int k, numBytes;
	uint8_t *buffer = NULL;
	struct SwsContext *sws_ctxRGB = NULL;

	// Allocate an AVFrame structure
	pFrameRGB = av_frame_alloc();
	if (pFrameRGB == NULL)
		return -1;

	// Determine required buffer size and allocate buffer
	numBytes = avpicture_get_size(PIX_FMT_RGB24, 1920, 1080);
	buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
	avpicture_fill((AVPicture *) pFrameRGB, buffer, PIX_FMT_RGB24, 1920, 1080);

	sws_ctxRGB = sws_getContext(1920, 1080, PIX_FMT_YUV422P, 1920, 1080,
			PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);

	AVFrame* pFrameYUV;
	pFrameYUV = av_frame_alloc();
	for (k = 0; k < numCacheFrame; k++) {
		pFrameYUV = videoCache[k];
		sws_scale(sws_ctxRGB, (uint8_t const * const *) pFrameYUV->data,
				pFrameYUV->linesize, 0, 1080, pFrameRGB->data,
				pFrameRGB->linesize);

		SaveFrame(pFrameRGB, 1920, 1080, k);
	}

	av_frame_free(&pFrameRGB);
	av_free(buffer);
	//free cache
	freeVideoCache();

	return 1;

	AVFormatContext *pFormatCtx = NULL;
	int i, videoStream;
	AVCodecContext *pCodecCtx = NULL;
	AVCodec *pCodec = NULL;
	AVFrame *pFrame = NULL;
	AVPacket packet;
	int frameFinished;
	//float           aspect_ratio;
	int m, n;

	AVDictionary *optionsDict = NULL;
	struct SwsContext *sws_ctx = NULL;

	SDL_Overlay *bmp = NULL;
	SDL_Surface *screen = NULL;
	SDL_Rect rect;
	SDL_Event event;

	if (argc < 2) {
		fprintf(stderr, "Usage: test <file>\n");
		exit(1);
	}
	// Register all formats and codecs
	av_register_all();

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
		exit(1);
	}

	// Open video file
	if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0) {
		printf("Can't open video \n");
		return -1; // Couldn't open file
	}
	// Retrieve stream information
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
		return -1; // Couldn't find stream information

	// Dump information about file onto standard error
	av_dump_format(pFormatCtx, 0, argv[1], 0);

	// Find the first video stream
	videoStream = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}
	if (videoStream == -1)
		return -1; // Didn't find a video stream

	// Get a pointer to the codec context for the video stream
	pCodecCtx = pFormatCtx->streams[videoStream]->codec;

	// Find the decoder for the video stream
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1; // Codec not found
	}

	// Open codec
	if (avcodec_open2(pCodecCtx, pCodec, &optionsDict) < 0)
		return -1; // Could not open codec

	// Allocate video frame
	pFrame = av_frame_alloc();

	// Make a screen to put our video
#ifndef __DARWIN__
	screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
#else
	screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 24, 0);
#endif
	if (!screen) {
		fprintf(stderr, "SDL: could not set video mode - exiting\n");
		exit(1);
	}

	// Allocate a place to put our YUV image on that screen
	bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height,
			SDL_YV12_OVERLAY, screen);

	sws_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
			pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
			PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);

	// Read frames and save first five frames to disk
	i = 0;
	while (av_read_frame(pFormatCtx, &packet) >= 0) {
		// Is this a packet from the video stream?
		if (packet.stream_index == videoStream) {
			// Decode video frame
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

			// Did we get a video frame?
			if (frameFinished) {
				SDL_LockYUVOverlay(bmp);

				AVPicture pict;
				pict.data[0] = bmp->pixels[0];
				pict.data[1] = bmp->pixels[2];
				pict.data[2] = bmp->pixels[1];

				pict.linesize[0] = bmp->pitches[0];
				pict.linesize[1] = bmp->pitches[2];
				pict.linesize[2] = bmp->pitches[1];

				// Convert the image into YUV format that SDL uses

				sws_scale(sws_ctx, (uint8_t const * const *) pFrame->data,
						pFrame->linesize, 0, pCodecCtx->height, pict.data,
						pict.linesize);

				SDL_UnlockYUVOverlay(bmp);

				rect.x = 0;
				rect.y = 0;
				rect.w = pCodecCtx->width;
				rect.h = pCodecCtx->height;
				SDL_DisplayYUVOverlay(bmp, &rect);

			}
		}

		// Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);
		SDL_PollEvent(&event);
		switch (event.type) {
		case SDL_QUIT:
			SDL_Quit();
			exit(0);
			break;
		default:
			break;
		}

	}

	// Free the YUV frame
	av_free(pFrame);

	// Close the codec
	avcodec_close(pCodecCtx);

	// Close the video file
	avformat_close_input(&pFormatCtx);

	return 0;
}

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
	FILE *pFile;
	char szFilename[32];
	int y;

	// Open file
	sprintf(szFilename, "frame%d.ppm", iFrame);
	pFile = fopen(szFilename, "wb");
	if (pFile == NULL)
		return;

	// Write header
	fprintf(pFile, "P6\n%d %d\n255\n", width, height);

	// Write pixel data
	for (y = 0; y < height; y++)
		fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, pFile);

	// Close file
	fclose(pFile);
}
