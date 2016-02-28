#include "looper.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Looper looper;

    looper.show();

    return app.exec();


}
