#include "looper.h"
#include "ui_looper.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "portaudio.h"

using namespace std;

#define SAMPLE_RATE  (44100)
#define FRAMES_PER_BUFFER (512)
#define MAX_NUM_SECONDS (30)
#define NUM_CHANNELS    (2)
#define DITHER_FLAG     (0)
#define WRITE_TO_FILE   (0)
#define SYNCH_TIME 5500

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

int globalFrameIndex = 0; //Global variable: tracks the first recording's current frame
int globalTrack0Length = 0; //Global variable: looks for how much the first recording will last


//Function to calculate the real synch time in case it becomes negative:
int calculateSynchTime(int start, int synchConst){
    int result = start - synchConst;
    if (result < 0){
        result = globalTrack0Length + result; //result is negative, so this is actually a subtraction.
    }
    return result;
}


static int recordCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
    /* Declaration of variables */

    paTestData *data = (paTestData*)userData;
    const SAMPLE *rptr = (const SAMPLE*)inputBuffer;
    SAMPLE *wptr = &data->recordedSamples[data->frameIndex * NUM_CHANNELS];
    long framesToCalc;
    long i;
    int finished;
    unsigned long framesLeft;
    (data->trackNumber == 0) ? framesLeft = data->maxFrameIndex - data->frameIndex : framesLeft = (globalTrack0Length-10) - data->frameIndex;
    (void) outputBuffer; // Prevent unused variable warnings.
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    /* It ensures that the buffer is terminated properly */

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
    if( inputBuffer == NULL ) // exceptional case, don't do anything here!!!
    {
        for( i=0; i<framesToCalc; i++ )
        {
            *wptr++ = SAMPLE_SILENCE;  // left
            if( NUM_CHANNELS == 2 ) *wptr++ = SAMPLE_SILENCE;  // right
        }
    }
    else
    {
        for( i=0; i<framesToCalc; i++ )
        {
            *wptr++ = *rptr++;  /* left */
            if( NUM_CHANNELS == 2 ) *wptr++ = *rptr++;  // right
            data->finalFrame++;
            if (data->trackNumber == 0) globalTrack0Length = data->finalFrame; //Updates the maximum playback time if this is the first track
        }
    }
    data->frameIndex += framesToCalc;
    data->finalFrame = data->frameIndex;
    if (data->trackNumber == 0) globalTrack0Length = data->finalFrame; //Updates the maximum playback time if this is the first track
    return finished;
}

static int playCallback( const void *inputBuffer, void *outputBuffer,
                        unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData )
{
    /* Declaration of variables */

    paTestData *data = (paTestData*)userData;
    SAMPLE *rptr = &data->recordedSamples[data->frameIndex * NUM_CHANNELS];
    SAMPLE *wptr = (SAMPLE*)outputBuffer;
    unsigned int i;
    int finished;
    unsigned int framesLeft = data->finalFrame - data->frameIndex;
    (void) inputBuffer; // Prevent unused variable warnings.
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    if ((data->isVirgin == true) && (data->trackNumber != 0)){     //First playback: makes sure it only starts playing when it's intended
        while (globalFrameIndex != 0){
            if (calculateSynchTime(data->startingFrame, SYNCH_TIME) >= globalFrameIndex){
                break;
            }
            continue;
        }
        data->isVirgin = false;
    }

    if (data->trackNumber != 0){ //Will not enter this loop if we're working with the first track.
        while ((calculateSynchTime(data->startingFrame, SYNCH_TIME) >= (globalFrameIndex)) && (data->frameIndex == 0)){ //Waits until the main track reaches the synch point for this track.
            continue;
        }
    }

    if( framesLeft < framesPerBuffer )
    {
        /* final buffer... */
        for( i=0; i<framesLeft; i++ )
        {
            *wptr++ = *rptr++;  // left
            if( NUM_CHANNELS == 2 ) *wptr++ = *rptr++;  // right
        }

        if (data->trackNumber != 0) {
            while (globalFrameIndex >= data->startingFrame){ //Waits until the main track has finished playing.
                continue;
            }
        }
        data->frameIndex = 0;
        finished = paContinue;
    }
    else
    {
        if (data->trackNumber == 0) globalFrameIndex = data->frameIndex; //Is this the main track? If so, update the global variable so other tracks can synch
        for( i=0; i<framesPerBuffer; i++ )
        {
            *wptr++ = *rptr++;  // left
            if( NUM_CHANNELS == 2 ) *wptr++ = *rptr++;  // right
        }
        data->frameIndex += framesPerBuffer;
        finished = paContinue;
    }
    return finished;


}

Looper::Looper(QWidget *parent) : QMainWindow(parent),ui(new Ui::Looper)
{

    ui->setupUi(this);
    err = Pa_Initialize();
    totalTracks = 0;
    QString TracksText = QString::number(totalTracks);
    ui->textEdit_2->setText("     "+TracksText);
    recording = false;
    paused = false;
    if( err != paNoError ) error(err);

    inputParameters.device = Pa_GetDefaultInputDevice(); // default input device
    if (inputParameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default input device.\n");
        error(err);
    }
    inputParameters.channelCount = 2;                    // stereo input
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    outputParameters.device = Pa_GetDefaultOutputDevice(); // default output device
    if (outputParameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default output device.\n");
        error(err);
    }
    outputParameters.channelCount = 2;                     // stereo output
    outputParameters.sampleFormat =  PA_SAMPLE_TYPE;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    //Startup button begining
    ui->pauseButton->setDisabled(true);
    ui->stopButton->setDisabled(true);
    //Startup ending
}

void Looper::startRecording(int index)
{
    /* Acquire parameters, allocate memory, clear it and start recording  */

    data[index].maxFrameIndex = totalFrames = MAX_NUM_SECONDS * SAMPLE_RATE; // Determines the size of the data
    data[index].frameIndex = 0;   // Clean the frame index to start recording
    data[index].trackNumber = index; //Saves which track this is
    (index == 0) ? data[0].startingFrame = 0 : data[index].startingFrame = data[0].frameIndex;
    numSamples = totalFrames * NUM_CHANNELS;
    numBytes = numSamples * sizeof(SAMPLE);
    data[index].recordedSamples = new SAMPLE[numBytes]; // From now on, recordedSamples is initialised.

    if( data[index].recordedSamples == NULL ) {
        cout << "Could not allocate record array." << endl << flush;
        error(err);
    }

    for( i=0; i<numSamples; i++ ) {
        data[index].recordedSamples[i] = 0;
    }

    /* The recording stream opens here */

    err = Pa_OpenStream(
                &stream[index],
                &inputParameters,
                NULL,                  // &outputParameters,
                SAMPLE_RATE,
                FRAMES_PER_BUFFER,
                paClipOff,             // we won't output out of range samples so don't bother clipping them
                recordCallback,
                &data[index] );
    if( err != paNoError ) error(err);


    /* Starts recording */

    err = Pa_StartStream(stream[index]);
    if( err != paNoError ) error(err);
}

void Looper::stopRecording(int index)
{
    /* Closes the recording stream */

    err = Pa_CloseStream( stream[index] );
    data[index].frameIndex = 0;
    if( err != paNoError ) error(err);
}

void Looper::startPlayback(int index)
{
   data[index].frameIndex = 0; // Clean the frame index to start playback
   data[index].isVirgin = true;

   /* The playback stream opens here */
   err = Pa_OpenStream(
               &stream[index],
               NULL,              // No input
               &outputParameters,
               SAMPLE_RATE,
               FRAMES_PER_BUFFER,
               paClipOff,         // We won't output out of range samples so don't bother clipping them
               playCallback,
               &data[index] );
   if( err != paNoError ) error(err);

   /* Starts playback */

   err = Pa_StartStream(stream[index]);
   if( err != paNoError ) error(err);
}

void Looper::pausePlayback()
{
    /* Function that pauses all audio tracks */

    for(int j=0;j<totalTracks;j++) {
        err = Pa_StopStream( stream[j] );
        if( err != paNoError ) error(err);
    }
}

void Looper::resumePlayback()
{
    /* Function that resumes all paused audio tracks */

    for(int j=0;j<totalTracks;j++) {
        err = Pa_StartStream(stream[j]);
        if( err != paNoError ) error(err);
    }
}

void Looper::stopPlayback()
{
    /* Function that stop the program and close all streams */

    for(int j=0;j<totalTracks;j++) {
        err = Pa_CloseStream(stream[j]);
        if( err != paNoError ) error(err);
    }
}


Looper::~Looper()
{
    /* Destructor to prevent memory leak */

    for (int k = 0; k < totalTracks; k++){
        if( data[k].recordedSamples ) delete data[k].recordedSamples;
    }
    delete ui;
}

PaError Looper::error(PaError err)
{
    /* Function to handle errors. It will appear throughout the code */

    Pa_Terminate();
    if( err != paNoError )
    {
        fprintf( stderr, "An error occured while using the portaudio stream\n" );
        fprintf( stderr, "Error number: %d\n", err );
        fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
        err = 1;          /* Always return 0 or 1, but no other return codes. */
    }
    this->~Looper();
    return err;
}

void Looper::on_recordButton_released()
{
    /* Qt Button configuration using aleready defined functions */

    if(recording == false) {
        startRecording(totalTracks);
        recording = true;
        ui->textEdit->setText("RECORDING");
        ui->MicLabel->setPixmap(QPixmap(":/Resources/MicrophoneNormal.png"));
        ui->recordButton->setIcon(QIcon(":/Resources/RecordPressed.png"));
        ui->recordButton->setText("Stop Recording");
        ui->pauseButton->setDisabled(true);
        ui->stopButton->setDisabled(true);

    }
    else {
        stopRecording(totalTracks);
        startPlayback(totalTracks);
        recording = false;
        ui->textEdit->setText("PLAYING BACK");
    ui->MicLabel->setPixmap(QPixmap(":/Resources/MicrophoneDisabled.png"));
        ui->recordButton->setIcon(QIcon(":/Resources/RecordNormal.png"));
        ui->recordButton->setText("Record");
        ui->pauseButton->setEnabled(true);
        ui->stopButton->setEnabled(true);
        totalTracks++;
        QString TracksText = QString::number(totalTracks);
        ui->textEdit_2->setText("     "+TracksText);
    }
}

void Looper::on_pauseButton_clicked()
{
    /* Qt Button configuration using aleready defined functions */

    if (paused == false) {
        pausePlayback();
        paused = true;
        ui->textEdit->setText("PAUSED");
        ui->recordButton->setDisabled(true);
    }
    else {
        resumePlayback();
        paused = false;
        ui->textEdit->setText("PLAYING BACK");
        ui->recordButton->setEnabled(true);
    }
}

void Looper::on_stopButton_clicked()
{
    /* Qt Button configuration using aleready defined functions */

    stopPlayback();
    ui->textEdit->setText("STOPPED");
}

void Looper::on_textEdit_destroyed()
{

}

void Looper::on_recordButton_clicked()
{

}
