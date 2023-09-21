#include "cube.h"
#include "oglwidget.h"

#include <QtOpenGL>
#include <GL/GLU.h>
#include <QtGlobal>
#include <QThread>
#include <algorithm>
#include <cfloat>

float g_fMinMouseLength = 0.5f;                                                 //minimalna dlugosc wektora przemieszczenia myszy potrzebna do obrocenia sekcji kostki
float g_fMaxMouseAngle = 135.0f;                                                //jak blisko kostki musi byc wektor przemieszczenia myszy, aby kostke obrocic
UINT  g_nRotationSteps = 150;                                                    //ilosc klatek animacji przy 90st. rotacji kostki

inline float toDegs(float fRadians)     {return fRadians*360/(2*M_PI);};

//---------------------------------------------------------------------------
QString cubeSideToString(SIDE s)
{
    switch (s) {
    case SD_RIGHT  : return "RIGHT";
    case SD_LEFT   : return "LEFT";
    case SD_TOP    : return "TOP";
    case SD_BOTTOM : return "BOTTOM";
    case SD_FRONT  : return "FRONT";
    case SD_BACK   : return "BACK";
    }

    return "";
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
float Vec2DDot(QVector2D& v1, QVector2D& v2)
{
return v1.x()*v2.x() + v1.y()*v2.y();
}

//---------------------------------------------------------------------------
float Vec2DAngle(QVector2D& v1, QVector2D& v2)                                  //Get angle in degrees between unit vectors v1 and v2
{
float d;
d = Vec2DDot(v1,v2);
if (d<-1.0f || d>1.0f) return 0.0/0.0;                                          //return NaN
return toDegs((float)acos(d));
}

//---------------------------------------------------------------------------
//Sprawdzanie po ktorej stronie linii znajduje sie zadany punkt
//Funkcja zwraca : 1 jesli punkt jest z prawej strony, 0 jesli punkt jest na linii, -1 jesli punkt jest z lewej strony
int lineTest(const PT2D& ptLineStart, const PT2D& ptLineEnd, const int ptX, const int ptY)
{
double dx = ptLineEnd.x - ptLineStart.x;
double dy = ptLineEnd.y - ptLineStart.y;
if (fabs(dx)>fabs(dy)) {
    double yOnline = dy/dx * (ptX - ptLineStart.x) + ptLineStart.y;
    return ptY >  yOnline ? 1 : (ptY < yOnline ? -1 : 0);
    }
else {
    double xOnline = dx/dy * (ptY - ptLineStart.y) + ptLineStart.x;
    return ptX >  xOnline ? 1 : (ptX < xOnline ? -1 : 0);
    }
}

//---------------------------------------------------------------------------
//Funkcja sprawdza czy zadany punkt miesci sie w zdanym czworokacie
bool poly4InsideTest(const PT2D* ptCorners, const int ptX, const int ptY)
{
int nResult1, nResult2;
nResult1 = lineTest(ptCorners[0], ptCorners[1], ptX, ptY);
nResult2 = lineTest(ptCorners[2], ptCorners[3], ptX, ptY);
if (((nResult1 > 0) && (nResult2 > 0)) || ((nResult1 < 0) && (nResult2 < 0))) return FALSE;
nResult1 = lineTest(ptCorners[0], ptCorners[3], ptX, ptY);
nResult2 = lineTest(ptCorners[1], ptCorners[2], ptX, ptY);
if (((nResult1 > 0) && (nResult2 > 0)) || ((nResult1 < 0) && (nResult2 < 0))) return FALSE;
return TRUE;
}

//---------------------------------------------------------------------------
//Funkcja sprawdza czy zadany punkt miesci sie w zdanym czwrokacie (przy tym ignorowania jest wspolrzedna z)
BOOL poly4InsideTest(PT3D* pt3Corners, const int ptX, const int ptY)
{
PT2D pt2Corners[4];
for (int i=0; i<4; i++) pt2Corners[i] = pt3Corners[i];
return poly4InsideTest(pt2Corners, ptX, ptY);
}

//---------------------------------------------------------------------------
//Funkcja sprawdza w ktorej sekcji kostki na osi X  miesci sie zadany punkt
UINT8 getXsection(PT2D* ptCorners, const int ptX, const int ptY)
{
int i, rc[cube_size];
double dx, dy;
PT2D ptLineStart, ptLineEnd;
dx = (ptCorners[1].x - ptCorners[0].x)/cube_size;                               //podziel zadany odcinek na ilosc sekcji
dy = (ptCorners[1].y - ptCorners[0].y)/cube_size;
//Now that we know point isn't in the center strip, test a line down the center
ptLineStart.x = ptCorners[0].x;                                                 //obliczamy pierwszy odcinek
ptLineStart.y = ptCorners[0].y;
ptLineEnd.x   = ptCorners[3].x;
ptLineEnd.y   = ptCorners[3].y;
for (i=0; i<cube_size; i++) {
    rc[i] = lineTest(ptLineStart, ptLineEnd, ptX, ptY);                         //wykonaj test polozenia punktu dla wszystkich otrzymanych odcinkow
    ptLineStart.x += dx;                                                        //obliczamy nastepny odcinek
    ptLineStart.y += dy;
    ptLineEnd.x   += dx;
    ptLineEnd.y   += dy;
    }
for (i=0; i<cube_size; i++) if (rc[i]==0) return i;                             //sprawdzamy czy punkt lezal na ktoryms z odcinkow
for (i=0; i<cube_size-1; i++) if (rc[i]!=rc[i+1]) return i;                     //sprawdzamy czy punkt lezy pomiedzy jakimis odcinkami poprzez sprawdzanie znakow
return cube_size-1;
}

//---------------------------------------------------------------------------
//Funkcja sprawdza w ktorej sekcji kostki na osi X  miesci sie zadany punkt
//W tym przypadku nastepuje zmiana wspolrzednych 3-wymiarowych na wspolrzedne 2-wymiarowe (ignorowane sa wspolrzedne z)
UINT8 getXsection(PT3D* pt3Corners, const int ptX, const int ptY)
{
PT2D pt2Corners[4];
for (int i=0; i<4; i++) pt2Corners[i] = pt3Corners[i];
return getXsection(pt2Corners, ptX, ptY);
}

//---------------------------------------------------------------------------
//Funkcja sprawdza w ktorej sekcji kostki na osi Y  miesci sie zadany punkt
UINT8 getYsection(PT2D* ptCorners, const int ptX, const int ptY)
{
int i, rc[cube_size];
double dx, dy;
PT2D ptLineStart, ptLineEnd;
dx = (ptCorners[2].x - ptCorners[1].x)/cube_size;                               //podziel zadany odcinek na ilosc sekcji
dy = (ptCorners[2].y - ptCorners[1].y)/cube_size;
ptLineStart.x = ptCorners[0].x;                                                 //obliczamy pierwszy odcinek
ptLineStart.y = ptCorners[0].y;
ptLineEnd.x   = ptCorners[1].x;
ptLineEnd.y   = ptCorners[1].y;
for (i=0; i<cube_size; i++) {
    rc[i] = lineTest(ptLineStart, ptLineEnd, ptX, ptY);                         //wykonaj test polozenia punktu dla wszystkich otrzymanych odcinkow
    ptLineStart.x += dx;                                                        //obliczamy nastepny odcinek
    ptLineStart.y += dy;
    ptLineEnd.x   += dx;
    ptLineEnd.y   += dy;
    }
for (i=0; i<cube_size; i++) if (rc[i]==0) return i;                             //sprawdzamy czy punkt lezal na ktoryms z odcinkow
for (i=0; i<cube_size-1; i++) if (rc[i]!=rc[i+1]) return i;                     //sprawdzamy czy punkt lezy pomiedzy jakimis odcinkami poprzez sprawdzanie znakow
return cube_size-1;
}
//---------------------------------------------------------------------------
//Funkcja sprawdza w ktorej sekcji kostki na osi Y miesci sie zadany punkt
//W tym przypadku nastepuje zmiana wspolrzednych 3-wymiarowych na wspolrzedne 2-wymiarowe (ignorowane sa wspolrzedne z)
UINT8 getYsection(PT3D* pt3Corners, const int ptX, const int ptY)
{
PT2D pt2Corners[4];
for (int i=0; i<4; i++) pt2Corners[i] = pt3Corners[i];
return getYsection(pt2Corners, ptX, ptY);
}

//---------------------------------------------------------------------------
//Tworzenie najmniejszego kawalka kostki
//Sprawdzane sa strony takiego kawalka i odpowiednio zaznaczane kolorem
//Kawalki lezace wewnatrz kostki beda oznaczone kolorem czarnym
TCubePiece::TCubePiece(BYTEVEC posHome)
{
    m_fRotationAngle = 0;
    m_vRotation.setX(0); m_vRotation.setY(0); m_vRotation.setZ(0);

    m_nSideColor[SD_RIGHT]  = posHome.x== (cube_size-1) ? WHITE : BLACK;
    m_nSideColor[SD_LEFT]   = posHome.x== 0             ? BLUE  : BLACK;
    m_nSideColor[SD_TOP]    = posHome.y== (cube_size-1) ? GREEN : BLACK;
    m_nSideColor[SD_BOTTOM] = posHome.y== 0             ? ORANGE : BLACK;
    m_nSideColor[SD_FRONT]  = posHome.z== (cube_size-1) ? RED    : BLACK;
    m_nSideColor[SD_BACK]   = posHome.z== 0             ? YELLOW : BLACK;
}

//---------------------------------------------------------------------------
void TCubePiece::RotateX(bool bCW)                                              //rotacja kawalka kostki na osi X
{
SIDECOLOR nTmp;
if (bCW) {                                                                      //czy rotacja wzgledem wskazowek zegara
    nTmp                    = m_nSideColor[SD_TOP];
    m_nSideColor[SD_TOP]    = m_nSideColor[SD_BACK];
    m_nSideColor[SD_BACK]   = m_nSideColor[SD_BOTTOM];
    m_nSideColor[SD_BOTTOM] = m_nSideColor[SD_FRONT];
    m_nSideColor[SD_FRONT]  = nTmp;
    }
else {
    nTmp                    = m_nSideColor[SD_TOP];
    m_nSideColor[SD_TOP]    = m_nSideColor[SD_FRONT];
    m_nSideColor[SD_FRONT]  = m_nSideColor[SD_BOTTOM];
    m_nSideColor[SD_BOTTOM] = m_nSideColor[SD_BACK];
    m_nSideColor[SD_BACK]   = nTmp;
    }
}

//---------------------------------------------------------------------------
void TCubePiece::RotateY(bool bCW)                                              //rotacja kawalka kostki na osi Y
{
SIDECOLOR nTmp;
if (bCW) {                                                                      //czy rotacja wzgledem wskazowek zegara
    nTmp                    = m_nSideColor[SD_FRONT];
    m_nSideColor[SD_FRONT]  = m_nSideColor[SD_LEFT];
    m_nSideColor[SD_LEFT]   = m_nSideColor[SD_BACK];
    m_nSideColor[SD_BACK]   = m_nSideColor[SD_RIGHT];
    m_nSideColor[SD_RIGHT]  = nTmp;
    }
else {
    nTmp                    = m_nSideColor[SD_FRONT];
    m_nSideColor[SD_FRONT]  = m_nSideColor[SD_RIGHT];
    m_nSideColor[SD_RIGHT]  = m_nSideColor[SD_BACK];
    m_nSideColor[SD_BACK]   = m_nSideColor[SD_LEFT];
    m_nSideColor[SD_LEFT]   = nTmp;
    }
}

//---------------------------------------------------------------------------
void TCubePiece::RotateZ(bool bCW)                                              //rotacja kawalka kostki na osi Z
{
SIDECOLOR nTmp;
if (bCW) {                                                                      //czy rotacja wzgledem wskazowek zegara
    nTmp                    = m_nSideColor[SD_TOP];
    m_nSideColor[SD_TOP]    = m_nSideColor[SD_RIGHT];
    m_nSideColor[SD_RIGHT]  = m_nSideColor[SD_BOTTOM];
    m_nSideColor[SD_BOTTOM] = m_nSideColor[SD_LEFT];
    m_nSideColor[SD_LEFT]   = nTmp;
    }
else {
    nTmp                    = m_nSideColor[SD_TOP];
    m_nSideColor[SD_TOP]    = m_nSideColor[SD_LEFT];
    m_nSideColor[SD_LEFT]   = m_nSideColor[SD_BOTTOM];
    m_nSideColor[SD_BOTTOM] = m_nSideColor[SD_RIGHT];
    m_nSideColor[SD_RIGHT]  = nTmp;
    }
}

//---------------------------------------------------------------------------
void TCubePiece::Draw(float x,float y,float z)                                  // rysowanie kawalka kostki
{
    glPushMatrix();                                                             // najpierw wykonujemy obrot takiego kawalka kostki (potrzebne dla animacji)
    if (m_fRotationAngle) glRotatef(m_fRotationAngle, m_vRotation.x(), m_vRotation.y(), m_vRotation.z());
    glTranslatef(x,y,z);                                                        // a nastepnie zaczynamy rysowac w odpowiednim miejscu
    glBegin(GL_QUADS);
        glColor3ub(MAKECOLOR(m_nSideColor[SD_RIGHT]));
        glVertex3f( 0.5f, 0.5f,-0.5f);					                        //Top Right Of The Quad (Right)
        glVertex3f( 0.5f, 0.5f, 0.5f);					                        //Top Left Of The Quad (Right)
        glVertex3f( 0.5f,-0.5f, 0.5f);					                        //Bottom Left Of The Quad (Right)
        glVertex3f( 0.5f,-0.5f,-0.5f);					                        //Bottom Right Of The Quad (Right)
        glColor3ub(MAKECOLOR(m_nSideColor[SD_BOTTOM]));
        glVertex3f( 0.5f,-0.5f, 0.5f);					                        //Top Right Of The Quad (Bottom)
        glVertex3f(-0.5f,-0.5f, 0.5f);					                        //Top Left Of The Quad (Bottom)
        glVertex3f(-0.5f,-0.5f,-0.5f);					                        //Bottom Left Of The Quad (Bottom)
        glVertex3f( 0.5f,-0.5f,-0.5f);					                        //Bottom Right Of The Quad (Bottom)
        glColor3ub(MAKECOLOR(m_nSideColor[SD_FRONT]));
        glVertex3f( 0.5f, 0.5f, 0.5f);					                        //Top Right Of The Quad (Front)
        glVertex3f(-0.5f, 0.5f, 0.5f);					                        //Top Left Of The Quad (Front)
        glVertex3f(-0.5f,-0.5f, 0.5f);					                        //Bottom Left Of The Quad (Front)
        glVertex3f( 0.5f,-0.5f, 0.5f);					                        //Bottom Right Of The Quad (Front)
        glColor3ub(MAKECOLOR(m_nSideColor[SD_TOP]));
        glVertex3f( 0.5f, 0.5f,-0.5f);					                        //Top Right Of The Quad (Top)
        glVertex3f(-0.5f, 0.5f,-0.5f);					                        //Top Left Of The Quad (Top)
        glVertex3f(-0.5f, 0.5f, 0.5f);					                        //Bottom Left Of The Quad (Top)
        glVertex3f( 0.5f, 0.5f, 0.5f);					                        //Bottom Right Of The Quad (Top)
        glColor3ub(MAKECOLOR(m_nSideColor[SD_LEFT]));
        glVertex3f(-0.5f, 0.5f, 0.5f);					                        // Top Right Of The Quad (Left)
        glVertex3f(-0.5f, 0.5f,-0.5f);					                        // Top Left Of The Quad (Left)
        glVertex3f(-0.5f,-0.5f,-0.5f);					                        // Bottom Left Of The Quad (Left)
        glVertex3f(-0.5f,-0.5f, 0.5f);					                        // Bottom Right Of The Quad (Left)
        glColor3ub(MAKECOLOR(m_nSideColor[SD_BACK]));
        glVertex3f( 0.5f,-0.5f,-0.5f);					                        // Top Right Of The Quad (Back)
        glVertex3f(-0.5f,-0.5f,-0.5f);					                        // Top Left Of The Quad (Back)
        glVertex3f(-0.5f, 0.5f,-0.5f);					                        // Bottom Left Of The Quad (Back)
        glVertex3f( 0.5f, 0.5f,-0.5f);					                        // Bottom Right Of The Quad (Back)
    glEnd();
    glColor3ub(MAKECOLOR(BLACK));
    if (m_nSideColor[SD_RIGHT]!=BLACK) {                                            //sprawdzamy czy kolor prawej strony kawalka kostki nie jest czarny
        glBegin(GL_LINE_LOOP);                                                      //jesli nie jest ro rysujemy obramowanie kwadratu
        glVertex3f( 0.5f, 0.5f,-0.5f);					                            //Top Right Of The Quad (Right)
        glVertex3f( 0.5f, 0.5f, 0.5f);					                            //Top Left Of The Quad (Right)
        glVertex3f( 0.5f,-0.5f, 0.5f);					                            //Bottom Left Of The Quad (Right)
        glVertex3f( 0.5f,-0.5f,-0.5f);					                            //Bottom Right Of The Quad (Right)
        glEnd();
    }
    if (m_nSideColor[SD_BOTTOM]!=BLACK) {                                           //sprawdzamy czy kolor spodniej strony kawalka kostki nie jest czarny
        glBegin(GL_LINE_LOOP);                                                      //jesli nie jest ro rysujemy obramowanie kwadratu
        glVertex3f( 0.5f,-0.5f, 0.5f);					                            //Top Right Of The Quad (Bottom)
        glVertex3f(-0.5f,-0.5f, 0.5f);					                            //Top Left Of The Quad (Bottom)
        glVertex3f(-0.5f,-0.5f,-0.5f);					                            //Bottom Left Of The Quad (Bottom)
        glVertex3f( 0.5f,-0.5f,-0.5f);					                            //Bottom Right Of The Quad (Bottom)
        glEnd();
    }
    if (m_nSideColor[SD_FRONT]!=BLACK) {                                            //sprawdzamy czy kolor przedniej strony kawalka kostki nie jest czarny
        glBegin(GL_LINE_LOOP);                                                      //jesli nie jest ro rysujemy obramowanie kwadratu
        glVertex3f( 0.5f, 0.5f, 0.5f);					                            //Top Right Of The Quad (Front)
        glVertex3f(-0.5f, 0.5f, 0.5f);					                            //Top Left Of The Quad (Front)
        glVertex3f(-0.5f,-0.5f, 0.5f);					                            //Bottom Left Of The Quad (Front)
        glVertex3f( 0.5f,-0.5f, 0.5f);					                            //Bottom Right Of The Quad (Front)
        glEnd();
    }
    if (m_nSideColor[SD_TOP]!=BLACK) {                                              //sprawdzamy czy kolor gornej strony kawalka kostki nie jest czarny
        glBegin(GL_LINE_LOOP);                                                      //jesli nie jest ro rysujemy obramowanie kwadratu
        glVertex3f( 0.5f, 0.5f,-0.5f);					                            //Top Right Of The Quad (Top)
        glVertex3f(-0.5f, 0.5f,-0.5f);					                            //Top Left Of The Quad (Top)
        glVertex3f(-0.5f, 0.5f, 0.5f);					                            //Bottom Left Of The Quad (Top)
        glVertex3f( 0.5f, 0.5f, 0.5f);					                            //Bottom Right Of The Quad (Top)
        glEnd();
    }
    if (m_nSideColor[SD_LEFT]!=BLACK) {
        glBegin(GL_LINE_LOOP);                                                      //sprawdzamy czy kolor lewej strony kawalka kostki nie jest czarny//jesli nie jest ro rysujemy obramowanie kwadratu
        glVertex3f(-0.5f, 0.5f, 0.5f);					                            //Top Right Of The Quad (Left)
        glVertex3f(-0.5f, 0.5f,-0.5f);					                            //Top Left Of The Quad (Left)
        glVertex3f(-0.5f,-0.5f,-0.5f);					                            //Bottom Left Of The Quad (Left)
        glVertex3f(-0.5f,-0.5f, 0.5f);					                            //Bottom Right Of The Quad (Left)
        glEnd();
    }
    if (m_nSideColor[SD_BACK]!=BLACK) {                                             //sprawdzamy czy kolor tylnej strony kawalka kostki nie jest czarny
        glBegin(GL_LINE_LOOP);                                                      //jesli nie jest ro rysujemy obramowanie kwadratu
        glVertex3f( 0.5f,-0.5f,-0.5f);					                            //Top Right Of The Quad (Back)
        glVertex3f(-0.5f,-0.5f,-0.5f);					                            //Top Left Of The Quad (Back)
        glVertex3f(-0.5f, 0.5f,-0.5f);					                            //Bottom Left Of The Quad (Back)
        glVertex3f( 0.5f, 0.5f,-0.5f);					                            //Bottom Right Of The Quad (Back)
        glEnd();
    }
    glPopMatrix();
}

//---------------------------------------------------------------------------
TCube::TCube(OGLWidget *widget)
{
    this->widget = widget;
    memset(m_pPieces, 0, sizeof(TCubePiece*)*cube_size*cube_size*cube_size);        //tworzymy zerowe wskazniki do kawalkow kostki
    Reset();
    //Random();
}

//---------------------------------------------------------------------------
TCube::~TCube()
{
for (int x=0; x<cube_size; x++) {
    for (int y=0; y<cube_size; y++) {
        for (int z=0; z<cube_size; z++) {
            if (m_pPieces[x][y][z]) delete m_pPieces[x][y][z];                  //tworzymy odpowiedni kawalek kostki i zapisujemy jego wskaznik w macierzy kawalkow
            }
        }
    }
}

//---------------------------------------------------------------------------
void TCube::Reset(void)                                                         //reset kostki - wszystkie kolory ulozone
{
for (int x=0; x<cube_size; x++) {
    for (int y=0; y<cube_size; y++) {
        for (int z=0; z<cube_size; z++) {
            if (m_pPieces[x][y][z]) delete m_pPieces[x][y][z];                  //jesli byly jakies kawalki kostki to je usun
            m_pPieces[x][y][z] = new TCubePiece(BYTEVEC(x,y,z));                //utworz nowe kawalki kostki w odpowiedniej pozycji
            }
        }
    }
}

//---------------------------------------------------------------------------
void TCube::Random(void)                                                        //losowe poprzedkladanie kostki
{
bool bCW;
UINT8 nSection, nAxis;

QTime time = QTime::currentTime();
qsrand((uint)time.msec());
for (int i=0; i<100; i++) {                                                     //przekladamy losowo sekcje kostki 100 razy
    bCW = (bool)(qrand() % 2);                                                   //losujemy czy rotacja ma byc wzgledem wskazowek zegara czy nie
    nSection =  qrand() % cube_size;                                             //losujemy numer sekcji
    nAxis = qrand() % 3;                                                         //losujemy os kostki
    if (nAxis==0) RotateXSection(nSection, bCW, FALSE);                         //wykonujemy przekladanie sekcji kostki na osi X
    else if (nAxis==1) RotateYSection(nSection, bCW, FALSE);                    //wykonujemy przekladanie sekcji kostki na osi Y
    else RotateZSection(nSection, bCW, FALSE);                                  //wykonujemy przekladanie sekcji kostki na osi Z
    }

blueEdgeOrientation = false;
}

//---------------------------------------------------------------------------
//Rotacja sekcji kostki na podstawie kierunku ruchu myszy i wielkosci okna wyswietlania
bool TCube::Rotate(GLdouble* mxProjection, GLdouble* mxModel, GLint* nViewPort,
            int wndSizeX, int wndSizeY, int ptMouseWndX, int ptMouseWndY, int ptLastMouseWndX, int ptLastMouseWndY, OGLWidget *widget)
{
int i;
double fTmp, fMinZ = DBL_MAX;
float xMin=FLT_MAX, yMin=FLT_MAX;
float xMax=FLT_MIN, yMax=FLT_MIN;
int ptMouseX=ptMouseWndX, ptMouseY=wndSizeY-ptMouseWndY;                        //Zmieniamy kierunek y myszy aby pasowal do wspolrzednych wyswietlania
int ptLastMouseX=ptLastMouseWndX, ptLastMouseY=wndSizeY-ptLastMouseWndY;

this->widget = widget;
SIDE nSide = (SIDE)-1;
//Na poczatku sprawdzamy czy mysz znajduje sie na kostce czy nie
//Zapisujemy do zmiennej wszystkie punkty krawedzi projektowanej kostki
PT3D vCubeCorner[8];
//Sciana przednia
gluProject( cube_size/2.0, cube_size/2.0, cube_size/2.0, mxModel, mxProjection, nViewPort, &vCubeCorner[0].x, &vCubeCorner[0].y, &vCubeCorner[0].z);
gluProject( cube_size/2.0,-cube_size/2.0, cube_size/2.0, mxModel, mxProjection, nViewPort, &vCubeCorner[1].x, &vCubeCorner[1].y, &vCubeCorner[1].z);
gluProject(-cube_size/2.0,-cube_size/2.0, cube_size/2.0, mxModel, mxProjection, nViewPort, &vCubeCorner[2].x, &vCubeCorner[2].y, &vCubeCorner[2].z);
gluProject(-cube_size/2.0, cube_size/2.0, cube_size/2.0, mxModel, mxProjection, nViewPort, &vCubeCorner[3].x, &vCubeCorner[3].y, &vCubeCorner[3].z);
//Sciana tylna
gluProject( cube_size/2.0, cube_size/2.0,-cube_size/2.0, mxModel, mxProjection, nViewPort, &vCubeCorner[4].x, &vCubeCorner[4].y, &vCubeCorner[4].z);
gluProject( cube_size/2.0,-cube_size/2.0,-cube_size/2.0, mxModel, mxProjection, nViewPort, &vCubeCorner[5].x, &vCubeCorner[5].y, &vCubeCorner[5].z);
gluProject(-cube_size/2.0,-cube_size/2.0,-cube_size/2.0, mxModel, mxProjection, nViewPort, &vCubeCorner[6].x, &vCubeCorner[6].y, &vCubeCorner[6].z);
gluProject(-cube_size/2.0, cube_size/2.0,-cube_size/2.0, mxModel, mxProjection, nViewPort, &vCubeCorner[7].x, &vCubeCorner[7].y, &vCubeCorner[7].z);

for (i=0; i<8; i++) {                                                           //obliczamy minimalne i maksymalne wspolrzedne X i Y dla sprawdzenia polozenia myszy
    xMin = std::min(xMin, (float)vCubeCorner[i].x);
    yMin = std::min(yMin, (float)vCubeCorner[i].y);
    xMax = std::max(xMax, (float)vCubeCorner[i].x);
    yMax = std::max(yMax, (float)vCubeCorner[i].y);
    }
if (!(xMin<=ptLastMouseX && ptLastMouseX<=xMax &&
      yMin<=ptLastMouseY && ptLastMouseY<=yMax)) {                              //sprawdzamy czy wskaznik myszy lezy na kostce
    return FALSE;                                                               //jesli nie to wychodzimy
    }
//Budujemy tablice skladajaca sie z wszystkich krawedzi kostki
PT3D vCorner[6][4] = {
    {vCubeCorner[5], vCubeCorner[1], vCubeCorner[0], vCubeCorner[4]},           //Prawa
    {vCubeCorner[6], vCubeCorner[2], vCubeCorner[3], vCubeCorner[7]},           //Lewa
    {vCubeCorner[7], vCubeCorner[4], vCubeCorner[0], vCubeCorner[3]},           //Gorna
    {vCubeCorner[6], vCubeCorner[5], vCubeCorner[1], vCubeCorner[2]},           //Dolna
    {vCubeCorner[2], vCubeCorner[1], vCubeCorner[0], vCubeCorner[3]},           //Przednia
    {vCubeCorner[6], vCubeCorner[5], vCubeCorner[4], vCubeCorner[7]},           //Tylna
    };

//sprawdzamy czy na ktorejs stronie kostki znajdowal sie wskzanik myszy
for (i=0; i<6; i++) {
    if (poly4InsideTest(vCorner[i], ptLastMouseX, ptLastMouseY)) {
        fTmp = std::min(std::min(std::min(vCorner[i][0].z, vCorner[i][1].z), vCorner[i][2].z), vCorner[i][3].z);
        if (fTmp < fMinZ) {
            nSide = (SIDE)i;
            fMinZ = fTmp;
            }
        }
    }

if (nSide==-1) return FALSE;                                                    //wskaznik myszy nie znajdowal sie na zadnej stronie kostki

//gdy juz mamy sprawdzona strone kostki, to sprawdzamy teraz kierunek rotacji sekcji
QVector2D vX((float)(vCorner[nSide][1].x - vCorner[nSide][0].x),(float)(vCorner[nSide][1].y - vCorner[nSide][0].y));
QVector2D vY((float)(vCorner[nSide][2].x - vCorner[nSide][1].x),(float)(vCorner[nSide][2].y - vCorner[nSide][1].y));
QVector2D vMouse((float)(ptMouseX-ptLastMouseX),(float)(ptMouseY-ptLastMouseY));    //obliczamy wektor przemieszczenia myszy
if (vMouse.length() < g_fMinMouseLength) return FALSE;                          //sprawdzamy czy ruch myszy nie jest za maly, jesli tak to anulujemy ten ruch
vX.normalize();                                                                 //sprawdzamy kierunek ruchu poprzez sprawdzenie kar
vY.normalize();                                                                 //wektora ruchu myszy
vMouse.normalize();                                                             //i wektora X/Y strony kostki

float xDiff, yDiff;
xDiff = Vec2DAngle(vX, vMouse); if (_isnan(xDiff)) return FALSE;
yDiff = Vec2DAngle(vY, vMouse); if (_isnan(yDiff)) return FALSE;
float minDiff = (float)std::min(std::min(std::min(fabs(xDiff), fabs(yDiff)), fabs(xDiff-180)), fabs(yDiff-180));
if (minDiff > g_fMaxMouseAngle) return FALSE;                                   //Jesli kata okaze sie za duzy do zakwalifikowania kierunku rotacji to go anulujemy

//Teraz sprawdzamy ktora sekcje kostki nalezy obrocic
UINT8 nSection;
minDiff += ALMOST_ZERO;                                                         //zwiekszamy wartosc w celu porownania z oryginalna wartoscia
if (fabs(xDiff) <= minDiff) {                                                   //sprawdzamy czy kat wektora ruchu myszy jest zgodny z ktromys z katow wektora stron
   nSection = getYsection(vCorner[nSide], ptLastMouseX, ptLastMouseY);          //obliczamy nr sekcji do obrocenia
   switch (nSide) {
      case SD_FRONT:    RotateYSection(nSection, TRUE, TRUE);  break;
      case SD_BACK:     RotateYSection(nSection, FALSE, TRUE); break;
      case SD_LEFT:     RotateYSection(nSection, TRUE, TRUE);  break;
      case SD_RIGHT:    RotateYSection(nSection, FALSE, TRUE); break;
      case SD_TOP:      RotateZSection(nSection, FALSE, TRUE); break;
      case SD_BOTTOM:   RotateZSection(nSection, TRUE, TRUE);  break;
   }
}
else if (fabs(xDiff-180) <= minDiff) {                                          //sprawdzamy czy kat wektora ruchu myszy jest zgodny z ktromys z katow wektora stron
   nSection = getYsection(vCorner[nSide], ptLastMouseX, ptLastMouseY);          //obliczamy nr sekcji do obrocenia
   switch (nSide) {
      case SD_FRONT:    RotateYSection(nSection, FALSE, TRUE); break;
      case SD_BACK:     RotateYSection(nSection, TRUE, TRUE);  break;
      case SD_LEFT:     RotateYSection(nSection, FALSE, TRUE); break;
      case SD_RIGHT:    RotateYSection(nSection, TRUE, TRUE);  break;
      case SD_TOP:      RotateZSection(nSection, TRUE, TRUE);  break;
      case SD_BOTTOM:   RotateZSection(nSection, FALSE, TRUE); break;
   }
}
else if (fabs(yDiff) <= minDiff) {                                              //sprawdzamy czy kat wektora ruchu myszy jest zgodny z ktromys z katow wektora stron
   nSection = getXsection(vCorner[nSide], ptLastMouseX, ptLastMouseY);          //obliczamy nr sekcji do obrocenia
   switch (nSide) {
      case SD_FRONT:    RotateXSection(nSection, FALSE, TRUE); break;
      case SD_BACK:     RotateXSection(nSection, TRUE,  TRUE); break;
      case SD_LEFT:     RotateZSection(nSection, FALSE, TRUE); break;
      case SD_RIGHT:    RotateZSection(nSection, TRUE, TRUE);  break;
      case SD_TOP:      RotateXSection(nSection, TRUE,  TRUE); break;
      case SD_BOTTOM:   RotateXSection(nSection, FALSE, TRUE); break;
   }
}
else if (fabs(yDiff-180) <= minDiff) {                                          //sprawdzamy czy kat wektora ruchu myszy jest zgodny z ktromys z katow wektora stron
   nSection = getXsection(vCorner[nSide], ptLastMouseX, ptLastMouseY);          //obliczamy nr sekcji do obrocenia
   switch (nSide) {
      case SD_FRONT:    RotateXSection(nSection, TRUE,  TRUE); break;
      case SD_BACK:     RotateXSection(nSection, FALSE, TRUE); break;
      case SD_LEFT:     RotateZSection(nSection, TRUE, TRUE);  break;
      case SD_RIGHT:    RotateZSection(nSection, FALSE, TRUE); break;
      case SD_TOP:      RotateXSection(nSection, FALSE, TRUE); break;
      case SD_BOTTOM:   RotateXSection(nSection, TRUE,  TRUE); break;
   }
}
else return false;
return TRUE;
}

//---------------------------------------------------------------------------
void TCube::RotateXSection(UINT8 nSection, BOOL bCW, BOOL bAnimate)             //rotacja sekcji kostki na osi X
{
int i, j, x=nSection, y, z, size;
TCubePiece* TmpPiece[cube_size-1];
float fAngle = bCW ? 90.0f : -90.0f;
if (nSection>=cube_size) return;
TCubePiece* pieces[cube_size*cube_size];
for (i=0, y=0; y<cube_size; y++)
    for (z=0; z<cube_size; z++) pieces[i++]=m_pPieces[x][y][z];                 //zapamietujemy ktore kawalki kostki beda obracane
if (bAnimate) AnimateRotation(pieces, ELEMENTS_OF(pieces), QVector3D(1,0,0), fAngle); //wykonujemy animacje obrotu tych kawalkow
for (i=0; i<ELEMENTS_OF(pieces); i++) pieces[i]->RotateX(bCW);                  //obkrecamy kazdy kawalek z osobna
if (bCW) {                                                                      //czy rotacja sekcji kostki zgodnie z kierunkiem wskazowek zegara
    size = cube_size-1;                                                         //przekladamy kawalki sekcji kostki zgodnie z kierunkiem wskazowek zegara
    for (j=0; j<cube_size/2; j++) {
        for (i=0; i<size; i++) TmpPiece[i] = m_pPieces[x][j][i+j];
        for (i=0; i<size; i++) m_pPieces[x][j][i+j] = m_pPieces[x][i+j][cube_size-1-j];
        for (i=0; i<size; i++) m_pPieces[x][i+j][cube_size-1-j] = m_pPieces[x][cube_size-1-j][cube_size-1-j-i];
        for (i=0; i<size; i++) m_pPieces[x][cube_size-1-j][cube_size-1-j-i] = m_pPieces[x][cube_size-1-j-i][j];
        for (i=0; i<size; i++) m_pPieces[x][cube_size-1-j-i][j] = TmpPiece[i];
        size -= 2;
        }
    }
else {                                                                          //przekladamy kawalki sekcji kostki przeciwnie do ruchu wskazowek zegara
    size = cube_size-1;
    for (j=0; j<cube_size/2; j++) {
        for (i=0; i<size; i++) TmpPiece[i] = m_pPieces[x][j][i+j];
        for (i=0; i<size; i++) m_pPieces[x][j][i+j] = m_pPieces[x][cube_size-1-j-i][j];
        for (i=0; i<size; i++) m_pPieces[x][cube_size-1-j-i][j] = m_pPieces[x][cube_size-1-j][cube_size-1-j-i];
        for (i=0; i<size; i++) m_pPieces[x][cube_size-1-j][cube_size-1-j-i] = m_pPieces[x][i+j][cube_size-1-j];
        for (i=0; i<size; i++) m_pPieces[x][i+j][cube_size-1-j] = TmpPiece[i];
        size -= 2;
        }
    }
}

//---------------------------------------------------------------------------
void TCube::RotateYSection(UINT8 nSection, BOOL bCW, BOOL bAnimate)             //rotacja sekcji kostki na osi Y
{
int i, j, x, y=nSection, z, size;
TCubePiece* TmpPiece[cube_size-1];
float fAngle = bCW ? 90.0f : -90.0f;
if (nSection>=cube_size) return;
TCubePiece* pieces[cube_size*cube_size];
for (i=0, x=0; x<cube_size; x++)
    for (z=0; z<cube_size; z++) pieces[i++]=m_pPieces[x][y][z];                 //zapamietujemy ktore kawalki kostki beda obracane
if (bAnimate) AnimateRotation(pieces, ELEMENTS_OF(pieces), QVector3D(0,1,0), fAngle); //wykonujemy animacje obrotu tych kawalkow
for (i=0; i<ELEMENTS_OF(pieces); i++) pieces[i]->RotateY(bCW);                  //obkrecamy kazdy kawalek z osobna
if (bCW) {                                                                      //czy rotacja sekcji kostki zgodnie z kierunkiem wskazowek zegara
    size = cube_size-1;                                                         //przekladamy kawalki sekcji kostki zgodnie z kierunkiem wskazowek zegara
    for (j=0; j<cube_size/2; j++) {
        for (i=0; i<size; i++) TmpPiece[i] = m_pPieces[j][y][i+j];
        for (i=0; i<size; i++) m_pPieces[j][y][i+j] = m_pPieces[cube_size-1-j-i][y][j];
        for (i=0; i<size; i++) m_pPieces[cube_size-1-j-i][y][j] = m_pPieces[cube_size-1-j][y][cube_size-1-j-i];
        for (i=0; i<size; i++) m_pPieces[cube_size-1-j][y][cube_size-1-j-i] = m_pPieces[i+j][y][cube_size-1-j];
        for (i=0; i<size; i++) m_pPieces[i+j][y][cube_size-1-j] = TmpPiece[i];
        size -= 2;
        }
    }
else {                                                                          //przekladamy kawalki sekcji kostki przeciwnie do ruchu wskazowek zegara
    size = cube_size-1;
    for (j=0; j<cube_size/2; j++) {
        for (i=0; i<size; i++) TmpPiece[i] = m_pPieces[j][y][i+j];
        for (i=0; i<size; i++) m_pPieces[j][y][i+j] = m_pPieces[i+j][y][cube_size-1-j];
        for (i=0; i<size; i++) m_pPieces[i+j][y][cube_size-1-j] = m_pPieces[cube_size-1-j][y][cube_size-1-j-i];
        for (i=0; i<size; i++) m_pPieces[cube_size-1-j][y][cube_size-1-j-i] = m_pPieces[cube_size-1-j-i][y][j];
        for (i=0; i<size; i++) m_pPieces[cube_size-1-j-i][y][j] = TmpPiece[i];
        size -= 2;
        }
    }
}

//---------------------------------------------------------------------------
void TCube::RotateZSection(UINT8 nSection, BOOL bCW, BOOL bAnimate)             //rotacja sekcji kostki na osi Z
{
int i, j, x, y, z=nSection, size;
TCubePiece* TmpPiece[cube_size-1];
float fAngle = bCW ? 90.0f : -90.0f;
if (nSection>=cube_size) return;
TCubePiece* pieces[cube_size*cube_size];
for (i=0, x=0; x<cube_size; x++)
    for (y=0; y<cube_size; y++) pieces[i++]=m_pPieces[x][y][z];                 //zapamietujemy ktore kawalki kostki beda obracane
if (bAnimate) AnimateRotation(pieces, ELEMENTS_OF(pieces), QVector3D(0,0,1), fAngle); //wykonujemy animacje obrotu tych kawalkow
for (i=0; i<ELEMENTS_OF(pieces); i++) pieces[i]->RotateZ(bCW);                  //obkrecamy kazdy kawalek z osobna
if (bCW) {                                                                      //czy rotacja sekcji kostki zgodnie z kierunkiem wskazowek zegara
    size = cube_size-1;                                                         //przekladamy kawalki sekcji kostki zgodnie z kierunkiem wskazowek zegara
    for (j=0; j<cube_size/2; j++) {
        for (i=0; i<size; i++) TmpPiece[i] = m_pPieces[j][i+j][z];
        for (i=0; i<size; i++) m_pPieces[j][i+j][z] = m_pPieces[i+j][cube_size-1-j][z];
        for (i=0; i<size; i++) m_pPieces[i+j][cube_size-1-j][z] = m_pPieces[cube_size-1-j][cube_size-1-j-i][z];
        for (i=0; i<size; i++) m_pPieces[cube_size-1-j][cube_size-1-j-i][z] = m_pPieces[cube_size-1-j-i][j][z];
        for (i=0; i<size; i++) m_pPieces[cube_size-1-j-i][j][z] = TmpPiece[i];
        size -= 2;
        }
    }
else {                                                                          //przekladamy kawalki sekcji kostki przeciwnie do ruchu wskazowek zegara
    size = cube_size-1;
    for (j=0; j<cube_size/2; j++) {
        for (i=0; i<size; i++) TmpPiece[i] = m_pPieces[j][i+j][z];
        for (i=0; i<size; i++) m_pPieces[j][i+j][z] = m_pPieces[cube_size-1-j-i][j][z];
        for (i=0; i<size; i++) m_pPieces[cube_size-1-j-i][j][z] = m_pPieces[cube_size-1-j][cube_size-1-j-i][z];
        for (i=0; i<size; i++) m_pPieces[cube_size-1-j][cube_size-1-j-i][z] = m_pPieces[i+j][cube_size-1-j][z];
        for (i=0; i<size; i++) m_pPieces[i+j][cube_size-1-j][z] = TmpPiece[i];
        size -= 2;
        }
    }
}

//---------------------------------------------------------------------------
void TCube::AnimateRotation(TCubePiece* piece[], int ctPieces, QVector3D v, float fAngle)
{
int i,x;

if (g_nRotationSteps <= 1) return;
for (i=0; (UINT)i<g_nRotationSteps; i++) {                                      //wykonuj w petli poszczegolne klatki animacji
    float fRotAngle = fAngle * i/g_nRotationSteps;                              //oblicz odpowiedni kat nachylenie sekcji kostki
    for (int x=0; x<ctPieces; x++) piece[x]->SetRotation(fRotAngle, v);         //i zapisz kat dla wszyskich kawalkow w tej sekcji kostki
    if (widget) {
        widget->updateGL();
    }
}
for (x=0; x<ctPieces; x++) piece[x]->ClrRotation();                             //na koncu animacji wyzeruj katy nachylenia wszyskich kawalkow z obracanej sekcji kostki
}

//---------------------------------------------------------------------------
void TCube::Draw(void)                                                          //rysowanie calej kostki
{
int x, y, z;
float posx, posy, posz;
posx = -(cube_size-1)/2.0;                                                      //liczymy poczatkowo pozycje x dla kawalka kostki
for (x=0; x<cube_size; x++) {
    posy = -(cube_size-1)/2.0;                                                  //liczymy poczatkowo pozycje y dla kawalka kostki
    for (y=0; y<cube_size; y++) {
        posz = -(cube_size-1)/2.0;                                              //liczymy poczatkowo pozycje z dla kawalka kostki
        for (z=0; z<cube_size; z++) {
            m_pPieces[x][y][z]->Draw(posx, posy, posz);                         //rysuj odpowiedni kawalek kostki w wyznaczonej pozycji
            posz += 1.0;                                                        //zwieksz pozycje z kawalka kostki
            }
        posy += 1.0;                                                            //zwieksz pozycje y kawalka kostki
        }
    posx += 1.0;                                                                //zwieksz pozycje x kawalka kostki
    }
}

//---------------------------------------------------------------------------
bool TCube::Check(void)                                                         //sprawdza czy kostka zostala ulozona
{
    int x, y, z;
    SIDECOLOR sidecolor;
    x=(cube_size-1);
    sidecolor = m_pPieces[x][0][0]->m_nSideColor[SD_RIGHT];
    for (y=0; y<cube_size; y++)
        for (z=0; z<cube_size; z++) if (m_pPieces[x][y][z]->m_nSideColor[SD_RIGHT]!=sidecolor) return false;
    x=0;
    sidecolor = m_pPieces[x][0][0]->m_nSideColor[SD_LEFT];
    for (y=0; y<cube_size; y++)
        for (z=0; z<cube_size; z++) if (m_pPieces[x][y][z]->m_nSideColor[SD_LEFT]!=sidecolor) return false;
    y=(cube_size-1);
    sidecolor = m_pPieces[0][y][0]->m_nSideColor[SD_TOP];
    for (x=0; x<cube_size; x++)
        for (z=0; z<cube_size; z++) if (m_pPieces[x][y][z]->m_nSideColor[SD_TOP]!=sidecolor) return false;
    y=0;
    sidecolor = m_pPieces[0][y][0]->m_nSideColor[SD_BOTTOM];
    for (x=0; x<cube_size; x++)
        for (z=0; z<cube_size; z++) if (m_pPieces[x][y][z]->m_nSideColor[SD_BOTTOM]!=sidecolor) return false;
    z=(cube_size-1);
    sidecolor = m_pPieces[0][0][z]->m_nSideColor[SD_FRONT];
    for (x=0; x<cube_size; x++)
        for (y=0; y<cube_size; y++) if (m_pPieces[x][y][z]->m_nSideColor[SD_FRONT]!=sidecolor) return false;
    z=0;
    sidecolor = m_pPieces[0][0][z]->m_nSideColor[SD_BACK];
    for (x=0; x<cube_size; x++)
        for (y=0; y<cube_size; y++) if (m_pPieces[x][y][z]->m_nSideColor[SD_BACK]!=sidecolor) return false;
    return true;
}

//---------------------------------------------------------------------------
SIDE TCube::FindWhiteCrossSide(void)
{
    if (m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT] == WHITE) return SD_RIGHT;
    if (m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT] == WHITE) return SD_LEFT;
    if (m_pPieces[cube_mid_pos][cube_size-1][cube_mid_pos]->m_nSideColor[SD_TOP] == WHITE) return SD_TOP;
    if (m_pPieces[cube_mid_pos][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM] == WHITE) return SD_BOTTOM;
    if (m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] == WHITE) return SD_FRONT;
    if (m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK] == WHITE) return SD_BACK;
    return (SIDE)-1;
}
//---------------------------------------------------------------------------
bool TCube::CheckWhiteCross(SIDE whiteCrossSide)
{
    switch (whiteCrossSide) {
    case SD_RIGHT  :
        if (m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT] != WHITE) return false;
        if (m_pPieces[cube_size-1][cube_size-1][cube_mid_pos]->m_nSideColor[SD_RIGHT] != WHITE) return false;
        if (m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_RIGHT] != WHITE) return false;
        if (m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_RIGHT] != WHITE) return false;
        break;
    case SD_LEFT   :
        if (m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT] != WHITE) return false;
        if (m_pPieces[0][cube_size-1][cube_mid_pos]->m_nSideColor[SD_LEFT] != WHITE) return false;
        if (m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_LEFT] != WHITE) return false;
        if (m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_LEFT] != WHITE) return false;
        break;
    case SD_TOP    :
        if (m_pPieces[0][cube_size-1][cube_mid_pos]->m_nSideColor[SD_TOP] != WHITE) return false;
        if (m_pPieces[cube_size-1][cube_size-1][cube_mid_pos]->m_nSideColor[SD_TOP] != WHITE) return false;
        if (m_pPieces[cube_mid_pos][cube_size-1][0]->m_nSideColor[SD_TOP] != WHITE) return false;
        if (m_pPieces[cube_mid_pos][cube_size-1][cube_size-1]->m_nSideColor[SD_TOP] != WHITE) return false;
        break;
    case SD_BOTTOM :
        if (m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM] != WHITE) return false;
        if (m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM] != WHITE) return false;
        if (m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM] != WHITE) return false;
        if (m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM] != WHITE) return false;
        break;
    case SD_FRONT  :
        if (m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] != WHITE) return false;
        if (m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] != WHITE) return false;
        if (m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT] != WHITE) return false;
        if (m_pPieces[cube_mid_pos][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT] != WHITE) return false;
        break;
    case SD_BACK   :
        if (m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_BACK] != WHITE) return false;
        if (m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_BACK] != WHITE) return false;
        if (m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK] != WHITE) return false;
        if (m_pPieces[cube_mid_pos][cube_size-1][0]->m_nSideColor[SD_BACK] != WHITE) return false;
        break;
    }
    return true;
}

//---------------------------------------------------------------------------
bool TCube::CheckWhiteCrossCorners(SIDE whiteCrossSide)
{
    switch (whiteCrossSide) {
    case SD_RIGHT  :
        if (m_pPieces[cube_size-1][cube_size-1][cube_size-1]->m_nSideColor[SD_RIGHT] != WHITE) return false;
        if (m_pPieces[cube_size-1][cube_size-1][0]->m_nSideColor[SD_RIGHT] != WHITE) return false;
        if (m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_RIGHT] != WHITE) return false;
        if (m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_RIGHT] != WHITE) return false;
        break;
    case SD_LEFT   :
        if (m_pPieces[0][cube_size-1][0]->m_nSideColor[SD_LEFT] != WHITE) return false;
        if (m_pPieces[0][cube_size-1][cube_size-1]->m_nSideColor[SD_LEFT] != WHITE) return false;
        if (m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_LEFT] != WHITE) return false;
        if (m_pPieces[0][0][0]->m_nSideColor[SD_LEFT] != WHITE) return false;
        break;
    case SD_TOP    :
        if (m_pPieces[0][cube_size-1][0]->m_nSideColor[SD_TOP] != WHITE) return false;
        if (m_pPieces[cube_size-1][cube_size-1][0]->m_nSideColor[SD_TOP] != WHITE) return false;
        if (m_pPieces[cube_size-1][cube_size-1][cube_size-1]->m_nSideColor[SD_TOP] != WHITE) return false;
        if (m_pPieces[0][cube_size-1][cube_size-1]->m_nSideColor[SD_TOP] != WHITE) return false;
        break;
    case SD_BOTTOM :
        if (m_pPieces[0][0][0]->m_nSideColor[SD_BOTTOM] != WHITE) return false;
        if (m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BOTTOM] != WHITE) return false;
        if (m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_BOTTOM] != WHITE) return false;
        if (m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_BOTTOM] != WHITE) return false;
        break;
    case SD_FRONT  :
        if (m_pPieces[0][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT] != WHITE) return false;
        if (m_pPieces[cube_size-1][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT] != WHITE) return false;
        if (m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_FRONT] != WHITE) return false;
        if (m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_FRONT] != WHITE) return false;
        break;
    case SD_BACK   :
        if (m_pPieces[cube_size-1][cube_size-1][0]->m_nSideColor[SD_BACK] != WHITE) return false;
        if (m_pPieces[0][cube_size-1][0]->m_nSideColor[SD_BACK] != WHITE) return false;
        if (m_pPieces[0][0][0]->m_nSideColor[SD_BACK] != WHITE) return false;
        if (m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BACK] != WHITE) return false;
        break;
    }
    return true;
}

//---------------------------------------------------------------------------
bool TCube::CheckSecondLayer(SIDE whiteCrossSide)
{
    SIDECOLOR color;
    color = m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT];
    if (color != m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_RIGHT]) return false;
    if (color != m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_RIGHT]) return false;

    color = m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT];
    if (color != m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_LEFT]) return false;
    if (color != m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_LEFT]) return false;

    color = m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT];
    if (color != m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) return false;
    if (color != m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) return false;

    color = m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK];
    if (color != m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_BACK]) return false;
    if (color != m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_BACK]) return false;

    return true;
}

//---------------------------------------------------------------------------
bool TCube::CheckBlueCross(SIDE whiteCrossSide)
{
    if (BLUE != m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM]) return false;
    if (BLUE != m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM]) return false;
    if (BLUE != m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM]) return false;
    if (BLUE != m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM]) return false;

    return true;
}

//---------------------------------------------------------------------------
bool TCube::CheckEdgePermutationOfBlueCross(SIDE whiteCrossSide)
{
    if (m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK] != m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK]) return false;
    if (m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT] != m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT]) return false;
    if (m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] != m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT]) return false;
    if (m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT] != m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT]) return false;

    return true;
}

//---------------------------------------------------------------------------
bool TCube::CheckPermutationOfBlueCorners(SIDE whiteCrossSide)
{
    SIDECOLOR color1, color2, color3;
    SIDECOLOR colorBottom, colorLeft, colorBack, colorRight, colorFront;

    colorBottom = BLUE;
    colorLeft   = m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT];
    colorBack   = m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK];
    colorRight  = m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT];
    colorFront  = m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT];

    color1 = m_pPieces[0][0][0]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[0][0][0]->m_nSideColor[SD_LEFT];
    color3 = m_pPieces[0][0][0]->m_nSideColor[SD_BACK];

    if (!((color1 == colorBottom) || (color1 == colorLeft) || (color1 == colorBack))) return false;
    if (!((color2 == colorBottom) || (color2 == colorLeft) || (color2 == colorBack))) return false;
    if (!((color3 == colorBottom) || (color3 == colorLeft) || (color3 == colorBack))) return false;

    color1 = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_RIGHT];
    color3 = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BACK];

    if (!((color1 == colorBottom) || (color1 == colorRight) || (color1 == colorBack))) return false;
    if (!((color2 == colorBottom) || (color2 == colorRight) || (color2 == colorBack))) return false;
    if (!((color3 == colorBottom) || (color3 == colorRight) || (color3 == colorBack))) return false;

    color1 = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_RIGHT];
    color3 = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_FRONT];

    if (!((color1 == colorBottom) || (color1 == colorRight) || (color1 == colorFront))) return false;
    if (!((color2 == colorBottom) || (color2 == colorRight) || (color2 == colorFront))) return false;
    if (!((color3 == colorBottom) || (color3 == colorRight) || (color3 == colorFront))) return false;

    color1 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_LEFT];
    color3 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_FRONT];

    if (!((color1 == colorBottom) || (color1 == colorLeft) || (color1 == colorFront))) return false;
    if (!((color2 == colorBottom) || (color2 == colorLeft) || (color2 == colorFront))) return false;
    if (!((color3 == colorBottom) || (color3 == colorLeft) || (color3 == colorFront))) return false;

    return true;
}

//---------------------------------------------------------------------------
bool TCube::CheckOrientationOfBlueCorners(SIDE whiteCrossSide)
{
    SIDECOLOR color1, color2, color3;
    SIDECOLOR colorBottom, colorLeft, colorBack, colorRight, colorFront;

    colorBottom = BLUE;
    colorLeft   = m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT];
    colorBack   = m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK];
    colorRight  = m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT];
    colorFront  = m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT];

    color1 = m_pPieces[0][0][0]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[0][0][0]->m_nSideColor[SD_LEFT];
    color3 = m_pPieces[0][0][0]->m_nSideColor[SD_BACK];

    if (color1 != colorBottom) return false;
    if (color2 != colorLeft) return false;
    if (color3 != colorBack) return false;

    color1 = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_RIGHT];
    color3 = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BACK];

    if (color1 != colorBottom) return false;
    if (color2 != colorRight) return false;
    if (color3 != colorBack) return false;

    color1 = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_RIGHT];
    color3 = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_FRONT];

    if (color1 != colorBottom) return false;
    if (color2 != colorRight) return false;
    if (color3 != colorFront) return false;

    color1 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_LEFT];
    color3 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_FRONT];

    if (color1 != colorBottom) return false;
    if (color2 != colorLeft) return false;
    if (color3 != colorFront) return false;

    return true;
}

//---------------------------------------------------------------------------
void TCube::WhiteCrossSideToTop(SIDE whiteCrossSide)
{
    switch (whiteCrossSide) {
    case SD_RIGHT  : RotateZSection(cube_mid_pos, TRUE, TRUE); break;
    case SD_LEFT   : RotateZSection(cube_mid_pos, FALSE, TRUE); break;
    case SD_TOP    : break;
    case SD_BOTTOM : RotateXSection(cube_mid_pos, TRUE, TRUE); break;
    case SD_FRONT  : RotateXSection(cube_mid_pos, FALSE, TRUE); break;
    case SD_BACK   : RotateXSection(cube_mid_pos, TRUE, TRUE); break;
    }
}

//---------------------------------------------------------------------------
void TCube::ArrangeWhiteCross(void)
{
    bool RotDownSide = false;
    SIDECOLOR color;

    // SD_BOTTOM
    if (m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM] == WHITE) {  //dol
        color = m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT];
        if (color == m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) {
            moves.append(ROT_F);
            moves.append(ROT_F);
            return;
        }
        else {
            RotDownSide = true;
        }
    }
    if (m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM] == WHITE) { // gora
        color = m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK];
        if (color == m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK]) {
            moves.append(ROT_B);
            moves.append(ROT_B);
            return;
        }
        else {
            RotDownSide = true;
        }
    }
    if (m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM] == WHITE) {  //prawo
        color = m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT];
        if (color == m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT]) {
            moves.append(ROT_R);
            moves.append(ROT_R);
            return;
        }
        else {
            RotDownSide = true;
        }
    }
    if (m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM] == WHITE) { // lewo
        color = m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT];
        if (color == m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT]) {
            moves.append(ROT_L);
            moves.append(ROT_L);
            return;
        }
        else {
            RotDownSide = true;
        }
    }

    if (RotDownSide) {
        moves.append(ROT_D);
        return;
    }

    // SD_TOP
    if (m_pPieces[cube_mid_pos][cube_size-1][cube_size-1]->m_nSideColor[SD_TOP] == WHITE) {  //dol
        color = m_pPieces[cube_mid_pos][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT];
        if (color != m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) {
            moves.append(ROT_F);
            moves.append(ROT_F);
            return;
        }
    }
    if (m_pPieces[cube_mid_pos][cube_size-1][0]->m_nSideColor[SD_TOP] == WHITE) { // gora
        color = m_pPieces[cube_mid_pos][cube_size-1][0]->m_nSideColor[SD_BACK];
        if (color != m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK]) {
            moves.append(ROT_B);
            moves.append(ROT_B);
            return;
        }
    }
    if (m_pPieces[cube_size-1][cube_size-1][cube_mid_pos]->m_nSideColor[SD_TOP] == WHITE) {  //prawo
        color = m_pPieces[cube_size-1][cube_size-1][cube_mid_pos]->m_nSideColor[SD_RIGHT];
        if (color != m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT]) {
            moves.append(ROT_R);
            moves.append(ROT_R);
            return;
        }
    }
    if (m_pPieces[0][cube_size-1][cube_mid_pos]->m_nSideColor[SD_TOP] == WHITE) { // lewo
        color = m_pPieces[0][cube_size-1][cube_mid_pos]->m_nSideColor[SD_LEFT];
        if (color != m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT]) {
            moves.append(ROT_L);
            moves.append(ROT_L);
            return;
        }
    }

    // SD_RIGHT
    if (m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT] == WHITE) {  //dol
        moves.append(ROT_R); moves.append(ROT_F); moves.append(ROT_D); moves.append(ROT_FCCW);
        return;
    }
    if (m_pPieces[cube_size-1][cube_size-1][cube_mid_pos]->m_nSideColor[SD_RIGHT] == WHITE) { // gora
        moves.append(ROT_R); moves.append(ROT_BCCW); moves.append(ROT_D); moves.append(ROT_B);
        return;
    }
    if (m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_RIGHT] == WHITE) {  //prawo
        color = m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_BACK];
        if (color == m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK]) {
            moves.append(ROT_B);
        }
        else {
            moves.append(ROT_BCCW);
            moves.append(ROT_D);
            moves.append(ROT_B);
        }
        return;
    }
    if (m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_RIGHT] == WHITE) { // lewo
        color = m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT];
        if (color == m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) {
            moves.append(ROT_FCCW);
        }
        else {
            moves.append(ROT_F);
            moves.append(ROT_D);
            moves.append(ROT_FCCW);
        }
        return;
    }

    // SD_LEFT
    if (m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT] == WHITE) {  //dol
        moves.append(ROT_L); moves.append(ROT_B); moves.append(ROT_D); moves.append(ROT_BCCW);
        return;
    }
    if (m_pPieces[0][cube_size-1][cube_mid_pos]->m_nSideColor[SD_LEFT] == WHITE) { // gora
        moves.append(ROT_L); moves.append(ROT_FCCW); moves.append(ROT_D); moves.append(ROT_F);
        return;
    }
    if (m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_LEFT] == WHITE) {  //prawo
        color = m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT];
        if (color == m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) {
            moves.append(ROT_F);
        }
        else {
            moves.append(ROT_FCCW);
            moves.append(ROT_D);
            moves.append(ROT_F);
        }
        return;
    }
    if (m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_LEFT] == WHITE) { // lewo
        color = m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_BACK];
        if (color == m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK]) {
            moves.append(ROT_BCCW);
        }
        else {
            moves.append(ROT_B);
            moves.append(ROT_D);
            moves.append(ROT_BCCW);
        }
        return;
    }

    // SD_FRONT
    if (m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT] == WHITE) {  //dol
        moves.append(ROT_F); moves.append(ROT_L); moves.append(ROT_D); moves.append(ROT_LCCW);
        return;
    }
    if (m_pPieces[cube_mid_pos][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT] == WHITE) { // gora
        moves.append(ROT_F); moves.append(ROT_RCCW); moves.append(ROT_D); moves.append(ROT_R);
        return;
    }
    if (m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] == WHITE) {  //prawo
        color = m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_RIGHT];
        if (color == m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT]) {
            moves.append(ROT_R);
        }
        else {
            moves.append(ROT_RCCW);
            moves.append(ROT_D);
            moves.append(ROT_R);
        }
        return;
    }
    if (m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] == WHITE) { // lewo
        color = m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_LEFT];
        if (color == m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT]) {
            moves.append(ROT_LCCW);
        }
        else {
            moves.append(ROT_L);
            moves.append(ROT_D);
            moves.append(ROT_LCCW);
        }
        return;
    }

    // SD_BACK
    if (m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK] == WHITE) {  //dol
        moves.append(ROT_B); moves.append(ROT_R); moves.append(ROT_D); moves.append(ROT_RCCW);
        return;
    }
    if (m_pPieces[cube_mid_pos][cube_size-1][0]->m_nSideColor[SD_BACK] == WHITE) { // gora
        moves.append(ROT_B); moves.append(ROT_LCCW); moves.append(ROT_D); moves.append(ROT_L);
        return;
    }
    if (m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_BACK] == WHITE) {  //prawo
        color = m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_LEFT];
        if (color == m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT]) {
            moves.append(ROT_L);
        }
        else {
            moves.append(ROT_LCCW);
            moves.append(ROT_D);
            moves.append(ROT_L);
        }
        return;
    }
    if (m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_BACK] == WHITE) { // lewo
        color = m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_RIGHT];
        if (color == m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT]) {
            moves.append(ROT_RCCW);
        }
        else {
            moves.append(ROT_R);
            moves.append(ROT_D);
            moves.append(ROT_RCCW);
        }
        return;
    }

}

//---------------------------------------------------------------------------
void TCube::ArrangeWhiteCrossCorners(void)
{
    SIDECOLOR color;
    bool RotDownSide = false;

    secondLayerBottomRotations = 0;

    // SD_RIGHT
    color = m_pPieces[cube_size-1][cube_size-1][cube_size-1]->m_nSideColor[SD_RIGHT];
    if ((color == WHITE) ||
        ((m_pPieces[cube_size-1][cube_size-1][cube_size-1]->m_nSideColor[SD_TOP] == WHITE) &&
         (m_pPieces[cube_size-1][cube_size-1][cube_mid_pos]->m_nSideColor[SD_RIGHT] != color)))  {  //TL
        moves.append(ROT_RCCW); moves.append(ROT_DCCW); moves.append(ROT_R);
        return;
    }
    color = m_pPieces[cube_size-1][cube_size-1][0]->m_nSideColor[SD_RIGHT];
    if ((color == WHITE) ||
        ((m_pPieces[cube_size-1][cube_size-1][0]->m_nSideColor[SD_TOP] == WHITE) &&
         (m_pPieces[cube_size-1][cube_size-1][cube_mid_pos]->m_nSideColor[SD_RIGHT] != color)))  {  //TR
        moves.append(ROT_R); moves.append(ROT_D); moves.append(ROT_RCCW);
        return;
    }
    if (m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_RIGHT] == WHITE) {  //BR
        color = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BACK];
        if (color == (m_pPieces[cube_mid_pos][cube_size-1][0]->m_nSideColor[SD_BACK])) {
            moves.append(ROT_DCCW); moves.append(ROT_BCCW); moves.append(ROT_D); moves.append(ROT_B);
            return;
        }
        else {
            RotDownSide = true;
        }
    }
    if (m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_RIGHT] == WHITE) { // BL
        color = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_FRONT];
        if (color == (m_pPieces[cube_mid_pos][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT])) {
            moves.append(ROT_D); moves.append(ROT_F); moves.append(ROT_DCCW); moves.append(ROT_FCCW);
            return;
        }
        else {
            RotDownSide = true;
        }
    }

    // SD_LEFT
    color = m_pPieces[0][cube_size-1][0]->m_nSideColor[SD_LEFT];
    if ((color == WHITE) ||
        ((m_pPieces[0][cube_size-1][0]->m_nSideColor[SD_TOP] == WHITE) &&
         (m_pPieces[0][cube_size-1][cube_mid_pos]->m_nSideColor[SD_LEFT] != color)))  {  //TL
        moves.append(ROT_LCCW); moves.append(ROT_DCCW); moves.append(ROT_L);
        return;
    }
    color = m_pPieces[0][cube_size-1][cube_size-1]->m_nSideColor[SD_LEFT];
    if ((color == WHITE) ||
        ((m_pPieces[0][cube_size-1][cube_size-1]->m_nSideColor[SD_TOP] == WHITE) &&
         (m_pPieces[0][cube_size-1][cube_mid_pos]->m_nSideColor[SD_LEFT] != color)))  {  //TR
        moves.append(ROT_L); moves.append(ROT_D); moves.append(ROT_LCCW);
        return;
    }
    if (m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_LEFT] == WHITE) {  //BR
        color = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_FRONT];
        if (color == (m_pPieces[cube_mid_pos][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT])) {
            moves.append(ROT_DCCW); moves.append(ROT_FCCW); moves.append(ROT_D); moves.append(ROT_F);
            return;
        }
        else {
            RotDownSide = true;
        }
    }
    if (m_pPieces[0][0][0]->m_nSideColor[SD_LEFT] == WHITE) { // BL
        color = m_pPieces[0][0][0]->m_nSideColor[SD_BACK];
        if (color == (m_pPieces[cube_mid_pos][cube_size-1][0]->m_nSideColor[SD_BACK])) {
            moves.append(ROT_D); moves.append(ROT_B); moves.append(ROT_DCCW); moves.append(ROT_BCCW);
            return;
        }
        else {
            RotDownSide = true;
        }
    }

    // SD_FRONT
    color = m_pPieces[0][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT];
    if ((color == WHITE) ||
        ((m_pPieces[0][cube_size-1][cube_size-1]->m_nSideColor[SD_TOP] == WHITE) &&
         (m_pPieces[cube_mid_pos][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT] != color)))  {  //TL
        moves.append(ROT_FCCW); moves.append(ROT_DCCW); moves.append(ROT_F);
        return;
    }
    color = m_pPieces[cube_size-1][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT];
    if ((color == WHITE) ||
        ((m_pPieces[cube_size-1][cube_size-1][cube_size-1]->m_nSideColor[SD_TOP] == WHITE) &&
         (m_pPieces[cube_mid_pos][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT] != color)))  {  //TR
        moves.append(ROT_F); moves.append(ROT_D); moves.append(ROT_FCCW);
        return;
    }
    if (m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_FRONT] == WHITE) {  //BR
        color = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_RIGHT];
        if (color == (m_pPieces[cube_size-1][cube_size-1][cube_mid_pos]->m_nSideColor[SD_RIGHT])) {
            moves.append(ROT_DCCW); moves.append(ROT_RCCW); moves.append(ROT_D); moves.append(ROT_R);
            return;
        }
        else {
            RotDownSide = true;
        }
    }
    if (m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_FRONT] == WHITE) { // BL
        color = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_LEFT];
        if (color == (m_pPieces[0][cube_size-1][cube_mid_pos]->m_nSideColor[SD_LEFT])) {
            moves.append(ROT_D); moves.append(ROT_L); moves.append(ROT_DCCW); moves.append(ROT_LCCW);
            return;
        }
        else {
            RotDownSide = true;
        }
    }

    // SD_BACK
    color = m_pPieces[cube_size-1][cube_size-1][0]->m_nSideColor[SD_BACK];
    if ((color == WHITE) ||
        ((m_pPieces[cube_size-1][cube_size-1][0]->m_nSideColor[SD_TOP] == WHITE) &&
         (m_pPieces[cube_mid_pos][cube_size-1][0]->m_nSideColor[SD_BACK] != color)))  {  //TL
        moves.append(ROT_BCCW); moves.append(ROT_DCCW); moves.append(ROT_B);
        return;
    }
    color = m_pPieces[0][cube_size-1][0]->m_nSideColor[SD_BACK];
    if ((color == WHITE) ||
        ((m_pPieces[0][cube_size-1][0]->m_nSideColor[SD_TOP] == WHITE) &&
         (m_pPieces[cube_mid_pos][cube_size-1][0]->m_nSideColor[SD_BACK] != color)))  {  //TR
        moves.append(ROT_B); moves.append(ROT_D); moves.append(ROT_BCCW);
        return;
    }
    if (m_pPieces[0][0][0]->m_nSideColor[SD_BACK] == WHITE) {  //BR
        color = m_pPieces[0][0][0]->m_nSideColor[SD_LEFT];
        if (color == (m_pPieces[0][cube_size-1][cube_mid_pos]->m_nSideColor[SD_LEFT])) {
            moves.append(ROT_DCCW); moves.append(ROT_LCCW); moves.append(ROT_D); moves.append(ROT_L);
            return;
        }
        else {
            RotDownSide = true;
        }
    }
    if (m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BACK] == WHITE) { // BL
        color = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_RIGHT];
        if (color == (m_pPieces[cube_size-1][cube_size-1][cube_mid_pos]->m_nSideColor[SD_RIGHT])) {
            moves.append(ROT_D); moves.append(ROT_R); moves.append(ROT_DCCW); moves.append(ROT_RCCW);
            return;
        }
        else {
            RotDownSide = true;
        }
    }

    if (RotDownSide) {
        moves.append(ROT_D);
        return;
    }

    // SD_BOTTOM
    if (m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_BOTTOM] == WHITE) {  //TL
        moves.append(ROT_FCCW); moves.append(ROT_D); moves.append(ROT_F);
        return;
    }
    if (m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_BOTTOM] == WHITE) { // TR
        moves.append(ROT_F); moves.append(ROT_DCCW); moves.append(ROT_FCCW);
        return;
    }
    if (m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BOTTOM] == WHITE) {  //BR
        moves.append(ROT_R); moves.append(ROT_D); moves.append(ROT_RCCW);
        return;
    }
    if (m_pPieces[0][0][0]->m_nSideColor[SD_BOTTOM] == WHITE) { // BL
        moves.append(ROT_LCCW); moves.append(ROT_DCCW); moves.append(ROT_L);
        return;
    }
}

//---------------------------------------------------------------------------
void TCube::ArrangeSecondLayer(void)
{
    SIDECOLOR color;
    SIDECOLOR color2;
    bool RotDownSide = false;

    color = m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT];

    if (secondLayerBottomRotations > 3) {
        if (color != m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_RIGHT]) {
            moves.append(ROT_DCCW); moves.append(ROT_BCCW); moves.append(ROT_D); moves.append(ROT_B);
            moves.append(ROT_RCCW); moves.append(ROT_B); moves.append(ROT_R); moves.append(ROT_BCCW);
            return;
        }
        if (color != m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_RIGHT]) {
            moves.append(ROT_D); moves.append(ROT_F); moves.append(ROT_DCCW); moves.append(ROT_FCCW);
            moves.append(ROT_R); moves.append(ROT_FCCW); moves.append(ROT_RCCW); moves.append(ROT_F);
            return;
        }
    }


    if (color == m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT]) {
        color = m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM];
        if (color != BLUE) {
            if (color == m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK]) {
                moves.append(ROT_DCCW); moves.append(ROT_BCCW); moves.append(ROT_D); moves.append(ROT_B);
                moves.append(ROT_RCCW); moves.append(ROT_B); moves.append(ROT_R); moves.append(ROT_BCCW);
                return;
            }
            if (color == m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) {
                moves.append(ROT_D); moves.append(ROT_F); moves.append(ROT_DCCW); moves.append(ROT_FCCW);
                moves.append(ROT_R); moves.append(ROT_FCCW); moves.append(ROT_RCCW); moves.append(ROT_F);
                return;
            }
        }
    }
    else {
        RotDownSide = true;
    }

    color = m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT];
    if (color == m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT]) {
        color = m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM];
        if (color != BLUE) {
            if (color == m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) {
                moves.append(ROT_DCCW); moves.append(ROT_FCCW); moves.append(ROT_D); moves.append(ROT_F);
                moves.append(ROT_LCCW); moves.append(ROT_F); moves.append(ROT_L); moves.append(ROT_FCCW);
                return;
            }
            if (color == m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK]) {
                moves.append(ROT_D); moves.append(ROT_B); moves.append(ROT_DCCW); moves.append(ROT_BCCW);
                moves.append(ROT_L); moves.append(ROT_BCCW); moves.append(ROT_LCCW); moves.append(ROT_B);
                return;
            }
        }
    }
    else {
        RotDownSide = true;
    }

    color = m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT];
    if (color == m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT]) {
        color = m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM];
        if (color != BLUE) {
            if (color == m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT]) {
                moves.append(ROT_DCCW); moves.append(ROT_RCCW); moves.append(ROT_D); moves.append(ROT_R);
                moves.append(ROT_FCCW); moves.append(ROT_R); moves.append(ROT_F); moves.append(ROT_RCCW);
                return;
            }
            if (color == m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT]) {
                moves.append(ROT_D); moves.append(ROT_L); moves.append(ROT_DCCW); moves.append(ROT_LCCW);
                moves.append(ROT_F); moves.append(ROT_LCCW); moves.append(ROT_FCCW); moves.append(ROT_L);
                return;
            }
        }
    }
    else {
        RotDownSide = true;
    }

    color = m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK];
    if (color == m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK]) {
        color = m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM];
        if (color != BLUE) {
            if (color == m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT]) {
                moves.append(ROT_DCCW); moves.append(ROT_LCCW); moves.append(ROT_D); moves.append(ROT_L);
                moves.append(ROT_BCCW); moves.append(ROT_L); moves.append(ROT_B); moves.append(ROT_LCCW);
                return;
            }
            if (color == m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT]) {
                moves.append(ROT_D); moves.append(ROT_R); moves.append(ROT_DCCW); moves.append(ROT_RCCW);
                moves.append(ROT_B); moves.append(ROT_RCCW); moves.append(ROT_BCCW); moves.append(ROT_R);
                return;
            }
        }
    }
    else {
        RotDownSide = true;
    }


    color = m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT];
    color2 = m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK];
    if (((color == m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_BACK]) &&
         (color2 == m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_RIGHT])) ||
        ((color == m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_RIGHT]) &&
         (color2 != m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_BACK]))) {
        moves.append(ROT_DCCW); moves.append(ROT_BCCW); moves.append(ROT_D); moves.append(ROT_B);
        moves.append(ROT_RCCW); moves.append(ROT_B); moves.append(ROT_R); moves.append(ROT_BCCW);
        return;
    }
    color2 = m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT];
    if (((color == m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) &&
         (color2 == m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_RIGHT])) ||
        ((color == m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_RIGHT]) &&
         (color2 != m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]))) {
        moves.append(ROT_D); moves.append(ROT_F); moves.append(ROT_DCCW); moves.append(ROT_FCCW);
        moves.append(ROT_R); moves.append(ROT_FCCW); moves.append(ROT_RCCW); moves.append(ROT_F);
        return;
    }

    color = m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT];
    color2 = m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT];
    if (((color == m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) &&
         (color2 == m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_LEFT]))  ||
        ((color == m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_LEFT]) &&
         (color2 != m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]))) {
        moves.append(ROT_DCCW); moves.append(ROT_FCCW); moves.append(ROT_D); moves.append(ROT_F);
        moves.append(ROT_LCCW); moves.append(ROT_F); moves.append(ROT_L); moves.append(ROT_FCCW);
        return;
    }
    color2 = m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK];
    if (((color == m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_BACK]) &&
         (color2 == m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_LEFT]))  ||
        ((color == m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_LEFT]) &&
         (color2 != m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_BACK]))) {
        moves.append(ROT_D); moves.append(ROT_B); moves.append(ROT_DCCW); moves.append(ROT_BCCW);
        moves.append(ROT_L); moves.append(ROT_BCCW); moves.append(ROT_LCCW); moves.append(ROT_B);
        return;
    }

    color = m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT];
    color2 = m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT];
    if (((color == m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_RIGHT]) &&
         (color2 == m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]))  ||
        ((color == m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) &&
         (color2 != m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_RIGHT]))) {
        moves.append(ROT_DCCW); moves.append(ROT_RCCW); moves.append(ROT_D); moves.append(ROT_R);
        moves.append(ROT_FCCW); moves.append(ROT_R); moves.append(ROT_F); moves.append(ROT_RCCW);
        return;
    }
    color2 = m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT];
    if (((color == m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_LEFT]) &&
         (color2 == m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]))  ||
        ((color == m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) &&
         (color2 != m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_LEFT]))) {
        moves.append(ROT_D); moves.append(ROT_L); moves.append(ROT_DCCW); moves.append(ROT_LCCW);
        moves.append(ROT_F); moves.append(ROT_LCCW); moves.append(ROT_FCCW); moves.append(ROT_L);
        return;
    }

    color = m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK];
    color2 = m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT];
    if (((color == m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_LEFT]) &&
         (color2 == m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_BACK]))  ||
        ((color == m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_BACK]) &&
         (color2 != m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_LEFT]))) {
        moves.append(ROT_DCCW); moves.append(ROT_LCCW); moves.append(ROT_D); moves.append(ROT_L);
        moves.append(ROT_BCCW); moves.append(ROT_L); moves.append(ROT_B); moves.append(ROT_LCCW);
        return;
    }
    color2 = m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT];
    if (((color == m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_RIGHT]) &&
         (color2 == m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_BACK]))  ||
        ((color == m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_BACK]) &&
         (color2 != m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_RIGHT]))) {
        moves.append(ROT_D); moves.append(ROT_R); moves.append(ROT_DCCW); moves.append(ROT_RCCW);
        moves.append(ROT_B); moves.append(ROT_RCCW); moves.append(ROT_BCCW); moves.append(ROT_R);
        return;
    }

    if (RotDownSide) {
        moves.append(ROT_D);
        secondLayerBottomRotations++;
        return;
    }
}

//---------------------------------------------------------------------------
void TCube::ArrangeBlueCross(void)
{
    if ((BLUE != m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM])) {

        moves.append(ROT_F); moves.append(ROT_L); moves.append(ROT_D);
        moves.append(ROT_LCCW); moves.append(ROT_DCCW); moves.append(ROT_FCCW);
        return;
    }

    if ((BLUE == m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE == m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM])) {

        moves.append(ROT_F); moves.append(ROT_L); moves.append(ROT_D);
        moves.append(ROT_LCCW); moves.append(ROT_DCCW); moves.append(ROT_FCCW);
        return;
    }

    if ((BLUE != m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE == m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE == m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM])) {

        moves.append(ROT_L); moves.append(ROT_B); moves.append(ROT_D);
        moves.append(ROT_BCCW); moves.append(ROT_DCCW); moves.append(ROT_LCCW);
        return;
    }

    if ((BLUE == m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE == m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM])) {

        moves.append(ROT_R); moves.append(ROT_F); moves.append(ROT_D);
        moves.append(ROT_FCCW); moves.append(ROT_DCCW); moves.append(ROT_RCCW);
        return;
    }

    if ((BLUE != m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE == m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE == m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM])) {

        moves.append(ROT_B); moves.append(ROT_R); moves.append(ROT_D);
        moves.append(ROT_RCCW); moves.append(ROT_DCCW); moves.append(ROT_BCCW);
        return;
    }

    if ((BLUE != m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE == m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE == m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM])) {

        moves.append(ROT_F); moves.append(ROT_L); moves.append(ROT_D);
        moves.append(ROT_LCCW); moves.append(ROT_DCCW); moves.append(ROT_FCCW);
        return;
    }

    if ((BLUE == m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE == m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM])) {

        moves.append(ROT_L); moves.append(ROT_B); moves.append(ROT_D);
        moves.append(ROT_BCCW); moves.append(ROT_DCCW); moves.append(ROT_LCCW);
        return;
    }
}

//---------------------------------------------------------------------------
void TCube::ArrangeEdgePermutationOfBlueCross(void)
{
    int cnt = 0;

    if (m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK] == m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK]) cnt++;
    if (m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT] == m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT]) cnt++;
    if (m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] == m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT]) cnt++;
    if (m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT] == m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT]) cnt++;

    if (cnt < 2) {
        moves.append(ROT_D);
        return;
    }

    if ((m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK] != m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK]) &&
        (m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT] != m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT]) &&
        (m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] == m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT]) &&
        (m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT] == m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT])) {
        moves.append(ROT_F); moves.append(ROT_D); moves.append(ROT_FCCW); moves.append(ROT_D);
        moves.append(ROT_F); moves.append(ROT_D); moves.append(ROT_D); moves.append(ROT_FCCW); moves.append(ROT_D);
        return;
    }

    if ((m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK] == m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK]) &&
        (m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT] != m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT]) &&
        (m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] != m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT]) &&
        (m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT] == m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT])) {
        moves.append(ROT_L); moves.append(ROT_D); moves.append(ROT_LCCW); moves.append(ROT_D);
        moves.append(ROT_L); moves.append(ROT_D); moves.append(ROT_D); moves.append(ROT_LCCW); moves.append(ROT_D);
        return;
    }

    if ((m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK] == m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK]) &&
        (m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT] == m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT]) &&
        (m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] != m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT]) &&
        (m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT] != m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT])) {
        moves.append(ROT_B); moves.append(ROT_D); moves.append(ROT_BCCW); moves.append(ROT_D);
        moves.append(ROT_B); moves.append(ROT_D); moves.append(ROT_D); moves.append(ROT_BCCW); moves.append(ROT_D);
        return;
    }

    if ((m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK] != m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK]) &&
        (m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT] == m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT]) &&
        (m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] == m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT]) &&
        (m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT] != m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT])) {
        moves.append(ROT_R); moves.append(ROT_D); moves.append(ROT_RCCW); moves.append(ROT_D);
        moves.append(ROT_R); moves.append(ROT_D); moves.append(ROT_D); moves.append(ROT_RCCW); moves.append(ROT_D);
        return;
    }

    if ((m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK] == m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK]) &&
        (m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] == m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT]) &&
        (m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT] != m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT]) &&
        (m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT] != m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT])) {
        moves.append(ROT_L); moves.append(ROT_D); moves.append(ROT_LCCW); moves.append(ROT_D);
        moves.append(ROT_L); moves.append(ROT_D); moves.append(ROT_D); moves.append(ROT_LCCW);
        return;
    }

    if ((m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK] != m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK]) &&
        (m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] != m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT]) &&
        (m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT] == m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT]) &&
        (m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT] == m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT])) {
        moves.append(ROT_F); moves.append(ROT_D); moves.append(ROT_FCCW); moves.append(ROT_D);
        moves.append(ROT_F); moves.append(ROT_D); moves.append(ROT_D); moves.append(ROT_FCCW);
        return;
    }

}

//---------------------------------------------------------------------------
void TCube::PermutationOfBlueCorners(void)
{
    SIDECOLOR color1, color2, color3;
    SIDECOLOR colorBottom, colorLeft, colorBack, colorRight, colorFront;

    colorBottom = BLUE;
    colorLeft   = m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT];
    colorBack   = m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK];
    colorRight  = m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT];
    colorFront  = m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT];

    color1 = m_pPieces[0][0][0]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[0][0][0]->m_nSideColor[SD_LEFT];
    color3 = m_pPieces[0][0][0]->m_nSideColor[SD_BACK];

    if (((color1 == colorBottom) || (color1 == colorLeft) || (color1 == colorBack)) &&
        ((color2 == colorBottom) || (color2 == colorLeft) || (color2 == colorBack)) &&
        ((color3 == colorBottom) || (color3 == colorLeft) || (color3 == colorBack))) {
        moves.append(ROT_D); moves.append(ROT_F); moves.append(ROT_DCCW); moves.append(ROT_BCCW);
        moves.append(ROT_D); moves.append(ROT_FCCW); moves.append(ROT_DCCW); moves.append(ROT_B);
        return;
    }

    color1 = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_RIGHT];
    color3 = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BACK];

    if (((color1 == colorBottom) || (color1 == colorRight) || (color1 == colorBack)) &&
        ((color2 == colorBottom) || (color2 == colorRight) || (color2 == colorBack)) &&
        ((color3 == colorBottom) || (color3 == colorRight) || (color3 == colorBack))) {
        moves.append(ROT_D); moves.append(ROT_R); moves.append(ROT_DCCW); moves.append(ROT_LCCW);
        moves.append(ROT_D); moves.append(ROT_RCCW); moves.append(ROT_DCCW); moves.append(ROT_L);
        return;
    }

    color1 = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_RIGHT];
    color3 = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_FRONT];

    if (((color1 == colorBottom) || (color1 == colorRight) || (color1 == colorFront)) &&
        ((color2 == colorBottom) || (color2 == colorRight) || (color2 == colorFront)) &&
        ((color3 == colorBottom) || (color3 == colorRight) || (color3 == colorFront))) {
        moves.append(ROT_D); moves.append(ROT_B); moves.append(ROT_DCCW); moves.append(ROT_FCCW);
        moves.append(ROT_D); moves.append(ROT_BCCW); moves.append(ROT_DCCW); moves.append(ROT_F);
        return;
    }

    color1 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_LEFT];
    color3 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_FRONT];

    if (((color1 == colorBottom) || (color1 == colorLeft) || (color1 == colorFront)) &&
        ((color2 == colorBottom) || (color2 == colorLeft) || (color2 == colorFront)) &&
        ((color3 == colorBottom) || (color3 == colorLeft) || (color3 == colorFront))) {
        moves.append(ROT_D); moves.append(ROT_L); moves.append(ROT_DCCW); moves.append(ROT_RCCW);
        moves.append(ROT_D); moves.append(ROT_LCCW); moves.append(ROT_DCCW); moves.append(ROT_R);
        return;
    }

    moves.append(ROT_D); moves.append(ROT_L); moves.append(ROT_DCCW); moves.append(ROT_RCCW);
    moves.append(ROT_D); moves.append(ROT_LCCW); moves.append(ROT_DCCW); moves.append(ROT_R);
}

//---------------------------------------------------------------------------
void TCube::OrientationOfBlueCorners(void)
{
    SIDECOLOR color1, color2, color3;
    SIDECOLOR colorBottom, colorLeft, colorBack, colorRight, colorFront;

    colorBottom = BLUE;
    colorLeft   = m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT];
    colorBack   = m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK];
    colorRight  = m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT];
    colorFront  = m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT];

    color1 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_LEFT];
    color3 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_FRONT];

    if ((color1 != colorBottom) || (color2 != colorLeft) || (color3 != colorFront)) {
        moves.append(ROT_LCCW); moves.append(ROT_UCCW); moves.append(ROT_L); moves.append(ROT_U);
        return;
    }

    moves.append(ROT_D);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
bool TCube::Solve(void)
{
    if (moves.length()) {
        switch (moves.at(0)) {
        case ROT_R    : RotateR(); break;
        case ROT_RCCW : RotateRCCW(); break;
        case ROT_L    : RotateL(); break;
        case ROT_LCCW : RotateLCCW(); break;
        case ROT_F    : RotateF(); break;
        case ROT_FCCW : RotateFCCW(); break;
        case ROT_B    : RotateB(); break;
        case ROT_BCCW : RotateBCCW(); break;
        case ROT_D    : RotateD(); break;
        case ROT_DCCW : RotateDCCW(); break;
        case ROT_U    : RotateU(); break;
        case ROT_UCCW : RotateUCCW(); break;
        }

        moves.erase(moves.begin()+0);
        return false;
    }

    if (blueEdgeOrientation) {
        OrientationOfBlueCorners();
        return Check();
    }

    SIDE whiteCrossSide = FindWhiteCrossSide();

    if (!CheckWhiteCross(whiteCrossSide)) {
        widget->setSolvingInterval(10);
        if (whiteCrossSide != SD_TOP) WhiteCrossSideToTop(whiteCrossSide);
        else ArrangeWhiteCross();
    }
    else {
        if (!CheckWhiteCrossCorners(whiteCrossSide)) {
            widget->setSolvingInterval(10);
            ArrangeWhiteCrossCorners();
        }
        else {
            if (!CheckSecondLayer(whiteCrossSide)) {
                widget->setSolvingInterval(10);
                ArrangeSecondLayer();
            }
            else {
                if (!CheckBlueCross(whiteCrossSide)) {
                    ArrangeBlueCross();
                }
                else {
                    if (!CheckEdgePermutationOfBlueCross(whiteCrossSide)) {
                        ArrangeEdgePermutationOfBlueCross();
                    }
                    else {
                        if (!CheckPermutationOfBlueCorners(whiteCrossSide)) {
                            widget->setSolvingInterval(500);
                            PermutationOfBlueCorners();
                        }
                        else {
                            if (!CheckOrientationOfBlueCorners(whiteCrossSide)) {
                                widget->setSolvingInterval(500);
                                blueEdgeOrientation = true;
                                OrientationOfBlueCorners();
                            }
                        }
                    }
                }
            }
        }

    }

    return Check();
}
//---------------------------------------------------------------------------
