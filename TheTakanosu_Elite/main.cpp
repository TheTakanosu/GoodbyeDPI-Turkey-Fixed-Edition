#include "TheTakanosu_Elite.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    TheTakanosu_Elite window;
    window.show();
    return app.exec();
}
