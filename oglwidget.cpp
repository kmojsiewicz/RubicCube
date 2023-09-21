#include <GL/gl.h>
#include <GL/glu.h>
#include "oglwidget.h"

#define SPHERE_DIAMETER         3*4.1f

float     g_fScale = 1.5f;                                                          // skala wyswietlania kostki


OGLWidget::OGLWidget(QWidget *parent)
    : QGLWidget(parent)
{
    mxRotation[0]=1.0f;                                                           // Kolumna 1 - zresetowanie polozenia kostki w przestrzeni
    mxRotation[1]=0.0f;
    mxRotation[2]=0.0f;
    mxRotation[3]=0.0f;
    mxRotation[4]=0.0f;                                                           // Kolumna 2
    mxRotation[5]=1.0f;
    mxRotation[6]=0.0f;
    mxRotation[7]=0.0f;
    mxRotation[8]=0.0;                                                            // Kolumna 3
    mxRotation[9]=0.0;
    mxRotation[10]=1.0;
    mxRotation[11]=0.0;
    mxRotation[12]=0.0;                                                           // Kolumna 4
    mxRotation[13]=0.0;
    mxRotation[14]=0.0;
    mxRotation[15]=1.0;

    cube = new TCube(this);
    solvingTimer = new QTimer();
    solvingTimer->setInterval(20);

    QObject::connect(solvingTimer, SIGNAL(timeout()), this, SLOT(solvingTimerTick()));
}

OGLWidget::~OGLWidget()
{
    delete cube;
}

void OGLWidget::solvingTimerTick()
{
//    if (solvingCnt > 10) {
//        solvingTimer->stop();
//        return;
//    }

    if (cube->Solve()) {
        solvingTimer->stop();
        return;
    }

    //solvingCnt++;
}

void OGLWidget::setSolvingInterval(int interval)
{
    solvingTimer->setInterval(interval);
}

void OGLWidget::on_pushButtonRandom_clicked()
{
    cube->Random();
    this->update();
}

void OGLWidget::on_pushButtonSolve_clicked()
{
    solvingCnt = 0;


    if (solvingTimer->isActive()) {
        solvingTimer->stop();
    }
    else {
        solvingTimer->start();
    }
}

void OGLWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        LMBPressPosition = QVector2D(e->localPos());
    }

    if (e->button() == Qt::RightButton) {
        this->setCursor(Qt::OpenHandCursor);
        RMBPressPosition = QVector2D(e->localPos());
    }
}

void OGLWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (e->buttons() & Qt::RightButton) {

        QVector2D diff = QVector2D(e->localPos()) - RMBPressPosition;
        QVector3D n = QVector3D(diff.y(), diff.x(), 0.0).normalized();              // Rotation axis is perpendicular to the mouse position difference vector

        qreal acc = diff.length();                                                  // Accelerate angular speed relative to the length of the mouse sweep
        rotationAxis = (rotationAxis * 0 + n * acc).normalized();                   // Calculate new rotation axis as weighted sum

        glPushMatrix();                                                             // gdy juz macierz modelu jest zaladowana
            glLoadIdentity();                                                       // ustawiamy ja na poczatkowe polozenie
            glRotatef(acc, rotationAxis.x(), rotationAxis.y(), rotationAxis.z());   // wykonujemy rotacje macierzy o zadany kat wokol obliczonego wczesniej wektora
            glMultMatrixf((GLfloat*)mxRotation);                                    // nastepnie przemnazamy przez macierz rotacji wczesniej zapisana
            glGetFloatv(GL_MODELVIEW_MATRIX, mxRotation);                           // i cala operacje zapisujemy na koniec do macierzy rotacji
        glPopMatrix();

        this->update();
        RMBPressPosition = QVector2D(e->localPos());
    }
}

void OGLWidget::mouseReleaseEvent(QMouseEvent *e)
{
    this->setCursor(Qt::ArrowCursor);

    if (e->button() == Qt::LeftButton) {

        GLint nViewPort[4];
        GLdouble mxProjection[16];

        if (e->x() != LMBPressPosition.x() || e->y() != LMBPressPosition.y()) {
            glGetDoublev(GL_PROJECTION_MATRIX, mxProjection);                       // odczytujemy macierz projekcji
            glGetIntegerv(GL_VIEWPORT, nViewPort);                                  // oraz wymiary okna wyswietlania i wykonujemy rotacje kostki
            if (cube->Rotate(mxProjection, mxLastModel, nViewPort,
                             this->width(), this->height(),
                             e->x(), e->y(),
                             LMBPressPosition.x(), LMBPressPosition.y(),
                             this)) {
                this->update();
            }
         }
    }
}

void OGLWidget::initializeGL()
{
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);							                                // The Type Of Depth Testing To Do
    glLineWidth(1.5);
    resizeGL(this->width(),this->height());
}

void OGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    qreal zNear  = 1.0f;                                                            // Pretty much always want this.
    qreal zFar   = zNear + SPHERE_DIAMETER;
    qreal left   = 0.0f - SPHERE_DIAMETER/2;
    qreal right  = 0.0f + SPHERE_DIAMETER/2;
    qreal bottom = 0.0f - SPHERE_DIAMETER/2;
    qreal top    = 0.0f + SPHERE_DIAMETER/2;
    qreal aspect = qreal(w)/qreal(h);
    if (aspect < 1.0) {                                                             // Fix the ortho project for aspect ratio
        bottom /= aspect;
        top /= aspect;
    }
    else {
        left *= aspect;
        right *= aspect;
    }
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(left, right, bottom, top, zNear, zFar);
    glMatrixMode(GL_MODELVIEW);
}

void OGLWidget::paintGL()
{
    glClearColor(0.39f,0.58f,0.93f,1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);                         // wyczysc okono kolerem i tla i ustaw bufor glebokosci

    glPushMatrix();
        gluLookAt(0,0,1+SPHERE_DIAMETER/2, 0,0,0, 0,1,0);                       // ustawienie kamery
        glScalef(g_fScale, g_fScale, g_fScale);                                 // skalowanie kostki
        glMultMatrixf((GLfloat*)mxRotation);                                    // ustawienie polozenia kostki w przestrzeni
        //glRotatef(30,1,0,0);                                                    // domyslnie kostka jest ustawiona pod katem 30st. do osi X
        //glRotatef(45,0,1,0);                                                    // domyslnie kostka jest ustawiona pod katem 45st. do osi Y
        glRotatef(20,1,0,0);                                                    // domyslnie kostka jest ustawiona pod katem 30st. do osi X
        glRotatef(-20,0,1,0);                                                    // domyslnie kostka jest ustawiona pod katem 45st. do osi Y
        glGetDoublev(GL_MODELVIEW_MATRIX, mxLastModel);                         // zapamietanie modelu wyswietlania
        cube->Draw();                                                           // rysowanie kostki
    glPopMatrix();

}


