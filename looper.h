#ifndef LOOPER_H
#define LOOPER_H

#include <QMainWindow>
#include <QMessageBox>
#include "portaudio.h"

#define SAMPLE_RATE  (44100)
#define FRAMES_PER_BUFFER (512)
#define NUM_SECONDS     (10)
#define NUM_CHANNELS    (2)
#define DITHER_FLAG     (0)
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

struct paTestData
{
    int          frameIndex;  /* Index into sample array. */
    int          maxFrameIndex;
    int          startingFrame;
    int          trackNumber;
    int          finalFrame;
    bool         isVirgin;
    SAMPLE      *recordedSamples;
};

namespace Ui {
class Looper;
}

class Looper : public QMainWindow
{
    Q_OBJECT

public:

    /*---------------------------------*/
    /* Structures and member variables */
    /*---------------------------------*/

    PaStreamParameters  inputParameters,
                        outputParameters;
    PaStream*           stream[1024];
    PaError             err;
    paTestData          data[1024];
    char                input;
    int                 Pa_GetVersion();
    int                 i;
    int                 totalTracks;
    int                 totalFrames;
    int                 numSamples;
    int                 numBytes;
    bool                recording;
    bool                paused;

    /*----------------*/
    /* Public methods */
    /*----------------*/
    explicit Looper(QWidget *parent = 0);
    void startRecording(int index);
    void stopRecording(int index);
    void startPlayback(int index);
    void pausePlayback();
    void resumePlayback();
    void stopPlayback();
    void savePlayback();
    void getDeviceInfo();
    static void PrintSupportedStandardSampleRates();

    PaError error(PaError err);
    ~Looper();


private slots:
    void on_pauseButton_clicked();

    void on_stopButton_clicked();

    void on_recordButton_clicked();

    void on_saveButton_clicked();

private:
    Ui::Looper *ui;
};

#endif // LOOPER_H
