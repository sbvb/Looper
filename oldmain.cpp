#ifdef _WIN32
    #include <windows.h>

    void sleep(unsigned milliseconds)
    {
        Sleep(milliseconds);
    }
#else
    #include <unistd.h>

    void sleep(unsigned milliseconds)
    {
        usleep(milliseconds * 1000); // takes microseconds
    }
#endif

using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "portaudio.h"
#include "mainwindow.h"
#include <QApplication>

/* #define SAMPLE_RATE  (17932) // Test failure to open with this value. */
#define SAMPLE_RATE  (44100)
#define FRAMES_PER_BUFFER (512)
#define NUM_SECONDS     (10)
#define NUM_CHANNELS    (2)
/* #define DITHER_FLAG     (paDitherOff) */
#define DITHER_FLAG     (0) /**/
/** Set to 1 if you want to capture the recording to a file. */
#define WRITE_TO_FILE   (0)

/* Select sample format. */
#if 1
#define PA_SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE  (0.0f)
#define PRINTF_S_FORMAT "%.8f"
#elif 1
#define PA_SAMPLE_TYPE  paInt16
typedef short SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"
#elif 0
#define PA_SAMPLE_TYPE  paInt8
typedef char SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"
#else
#define PA_SAMPLE_TYPE  paUInt8
typedef unsigned char SAMPLE;
#define SAMPLE_SILENCE  (128)
#define PRINTF_S_FORMAT "%d"
#endif

typedef struct
{
    int          frameIndex;  /* Index into sample array. */
    int          maxFrameIndex;
    SAMPLE      *recordedSamples;
}
paTestData;

/* This routine will be called by the PortAudio engine when audio is needed.
** It may be called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int recordCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
    paTestData *data = (paTestData*)userData;
    const SAMPLE *rptr = (const SAMPLE*)inputBuffer;
    SAMPLE *wptr = &data->recordedSamples[data->frameIndex * NUM_CHANNELS];
    long framesToCalc;
    long i;
    int finished;
    unsigned long framesLeft = data->maxFrameIndex - data->frameIndex;

    (void) outputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    if( framesLeft < framesPerBuffer )
    {
        framesToCalc = framesLeft;
        finished = paComplete;
    }
    else
    {
        framesToCalc = framesPerBuffer;
        finished = paContinue;
    }

    if( inputBuffer == NULL )
    {
        for( i=0; i<framesToCalc; i++ )
        {
            *wptr++ = SAMPLE_SILENCE;  /* left */
            if( NUM_CHANNELS == 2 ) *wptr++ = SAMPLE_SILENCE;  /* right */
        }
    }
    else
    {
        for( i=0; i<framesToCalc; i++ )
        {
            *wptr++ = *rptr++;  /* left */
            if( NUM_CHANNELS == 2 ) *wptr++ = *rptr++;  /* right */
        }
    }
    data->frameIndex += framesToCalc;
    return finished;
}

char* stringMultiplier(char multiplying, int multiplicator){ //This function forms strings composed of the same character.
    int i;
    static char r[100];
    for (i = 0; i<100; i++){
        r[i] = '\0';
    }
    if (multiplying <= 0){
        return r;
    } else {
        for (i=0; i<multiplicator; i++){
            r[i] = multiplying;
        }
    }
    return r;
}

void invokeWaitBeeps(int beeps, int totalMiliseconds, char mode){
    int k = beeps;
    int temp;
    int miliseconds = totalMiliseconds/beeps;
    cout << "|*" << stringMultiplier(' ', k-1) << "|" << ((mode == 'p') ? " <-RECORD HERE" : "  RECORDING...") << flush;
    sleep(miliseconds);
    for (k = beeps; k>1; k--){
        temp = k;
        temp = k+14;
        cout << stringMultiplier('\b', temp) << "*" << flush;
        cout << stringMultiplier(' ', k-2) << "|" << ((mode == 'p') ? " <-RECORD HERE" : "  RECORDING...")<< flush;
        sleep(miliseconds);
    }
    if (mode == 'p') {
        cout << stringMultiplier('\b', beeps+16) << flush;
    } else if (mode == 'r'){
        cout << stringMultiplier('\b', 12) << "    DONE!   " << flush;
    }
}

/* This routine will be called by the PortAudio engine when audio is needed.
** It may be called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int playCallback( const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData )
{
    paTestData *data = (paTestData*)userData;
    SAMPLE *rptr = &data->recordedSamples[data->frameIndex * NUM_CHANNELS];
    SAMPLE *wptr = (SAMPLE*)outputBuffer;
    unsigned int i;
    int finished;
    unsigned int framesLeft = data->maxFrameIndex - data->frameIndex;

    (void) inputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    if( framesLeft < framesPerBuffer )
    {
        /* final buffer... */
        for( i=0; i<framesLeft; i++ )
        {
            *wptr++ = *rptr++;  /* left */
            if( NUM_CHANNELS == 2 ) *wptr++ = *rptr++;  /* right */
        }

        data->frameIndex += framesLeft;
        if (framesLeft <= 100){
            data->frameIndex = 0;
        }
        finished = paContinue;
    }
    else
    {
        for( i=0; i<framesPerBuffer; i++ )
        {
            *wptr++ = *rptr++;  /* left */
            if( NUM_CHANNELS == 2 ) *wptr++ = *rptr++;  /* right */
        }
        data->frameIndex += framesPerBuffer;
        finished = paContinue;
    }
    return finished;
}

/*******************************************************************/

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    PaStreamParameters  inputParameters,
                        outputParameters;
    PaStream*           stream[1024];
    PaError             err = paNoError;
    paTestData          data[1024];
    char                input;
    int                 Pa_GetVersion ();
    int                 i;
    int                 k = 0;
    int                 totalFrames;
    int                 numSamples;
    int                 numBytes;
    SAMPLE              max, val;
    double              average;


    cout << "AUDIO LOOPER" << endl << endl
         << "Press 'r' when you wish to record a track, 'p' to pause playback and 's' to stop the program."
            " You may record up to 6 tracks." << endl
         << "When you do, pay close attention to the metronome and start recording"
            " when it reaches its end." << endl;

    w.show();
    err = Pa_Initialize();
    if( err != paNoError ) goto done;

    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    if (inputParameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default input device.\n");
        goto done;
    }
    inputParameters.channelCount = 2;                    /* stereo input */
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    /* Record some audio. -------------------------------------------- */

    do{
        data[k].maxFrameIndex = totalFrames = NUM_SECONDS * SAMPLE_RATE; /* Record for a few seconds. */
        data[k].frameIndex = 0;

        numSamples = totalFrames * NUM_CHANNELS;
        numBytes = numSamples * sizeof(SAMPLE);

        data[k].recordedSamples = (SAMPLE *) malloc( numBytes ); /* From now on, recordedSamples is initialised. */
        if( data[k].recordedSamples == NULL ) {
            cout << "Could not allocate record array." << endl << flush;
            goto done;
        }
        for( i=0; i<numSamples; i++ ) {
            data[k].recordedSamples[i] = 0;
        }

        cin >> input;   /* Waits until the user presses r*/

        err = Pa_OpenStream(
                  &stream[k],
                  &inputParameters,
                  NULL,                  /* &outputParameters, */
                  SAMPLE_RATE,
                  FRAMES_PER_BUFFER,
                  paClipOff,      /* we won't output out of range samples so don't bother clipping them */
                  recordCallback,
                  &data[k] );
        if( err != paNoError ) goto done;

        if (input=='s'){
            break;
        }

        if (input == 'p') {
            for(int j=0;j<k;j++) {
                err = Pa_StopStream( stream[j] );
                if( err != paNoError ) goto done;
            }
            cout << "Press '0' and type Enter to resume playing." << endl;
            cin >> input;
            if (input == '0') {
                for(int j=0;j<k;j++) {
                    err = Pa_StartStream( stream[j] );
                    if( err != paNoError ) goto done;
                }
            }
            continue;
        }

        if(input=='r') {

            cout << endl << flush;    /* Calls invokeWaitBeeps in pre-recording mode*/
            invokeWaitBeeps(8, NUM_SECONDS*500, 'p');

            err = Pa_StartStream( stream[k] );     /*Starts recording*/
            if( err != paNoError ) goto done;

            invokeWaitBeeps(8, NUM_SECONDS*1000, 'r');     /* Calls invokeWaitBeeps in recording mode */

            err = Pa_CloseStream( stream[k] );    /*Closes the recording stream*/
            if( err != paNoError ) goto done;

            /* Playback recorded data.  -------------------------------------------- */

            data[k].frameIndex = 0;

            outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
            if (outputParameters.device == paNoDevice) {
                fprintf(stderr,"Error: No default output device.\n");
                goto done;
            }
            outputParameters.channelCount = 2;                     /* stereo output */
            outputParameters.sampleFormat =  PA_SAMPLE_TYPE;
            outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
            outputParameters.hostApiSpecificStreamInfo = NULL;

            err = Pa_OpenStream(
                      &stream[k],
                      NULL, /* no input */
                      &outputParameters,
                      SAMPLE_RATE,
                      FRAMES_PER_BUFFER,
                      paClipOff,      /* we won't output out of range samples so don't bother clipping them */
                      playCallback,
                      &data[k] );
            if( err != paNoError ) goto done;

            err = Pa_StartStream( stream[k] );
            if( err != paNoError ) goto done;

            ++k;
        }
    } while (k < 6);

    if( err < 0 ) goto done;

    for(int j=0;j<k;j++) {    //Close all streams
        err = Pa_CloseStream( stream[j] );
        if( err != paNoError ) goto done;
    }
    cout << "Stopped." << endl;

    return a.exec();

done:
    Pa_Terminate();
    for (k = 0; k < 2; k++){
        if( data[k].recordedSamples )       /* Sure it is NULL or valid. */
            free( data[k].recordedSamples );
    }
    if( err != paNoError )
    {
        fprintf( stderr, "An error occured while using the portaudio stream\n" );
        fprintf( stderr, "Error number: %d\n", err );
        fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
        err = 1;          /* Always return 0 or 1, but no other return codes. */
    }
    return err;
}
