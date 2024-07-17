#include "camera.h"
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

// Cette fonction dessine une ligne infinie entre deux points sur une image
void drawInfiniteLine(cv::Mat &image, cv::Point pt1, cv::Point pt2, cv::Scalar color, int thickness)
{
    // Calcul de la pente de la ligne
    double slope = (pt2.y - pt1.y) / (double)(pt2.x - pt1.x);
    // Calcul de l'ordonnée à l'origine
    double y_intercept = pt1.y - slope * pt1.x;

    // Obtention des dimensions de l'image
    int imgWidth = image.cols;
    int imgHeight = image.rows;

    // Extension des points pour la ligne infinie
    cv::Point pt1_extended, pt2_extended;
    pt1_extended.x = 0;
    pt1_extended.y = y_intercept;
    pt2_extended.x = imgWidth - 1;
    pt2_extended.y = slope * pt2_extended.x + y_intercept;

    // Dessin de la ligne sur l'image
    cv::line(image, pt1_extended, pt2_extended, color, thickness, cv::LINE_AA);
}

// Cette fonction crée une zone d'intérêt et détecte le passage de véhicules
void zone(int x, int y, int w, int h, float angle, cv::Mat draw, cv::Mat img, int &pixImg, int &compt, float up, float down)
{
    // Création d'un rectangle orienté selon un angle
    RotatedRect rRect = RotatedRect(Point2f(x, y), Size2f(w, h), angle);

    // Transformation pour obtenir la zone d'intérêt
    Mat M, rotated, cropped;
    M = getRotationMatrix2D(rRect.center, rRect.angle, 1.0);
    warpAffine(img, rotated, M, img.size(), INTER_CUBIC);
    getRectSubPix(rotated, rRect.size, rRect.center, cropped);

    // Dessin du rectangle sur l'image
    Point2f vertices[4];
    rRect.points(vertices);
    for (int i = 0; i < 4; i++)
        line(draw, vertices[i], vertices[(i + 1) % 4], Scalar(0, 255, 0), 2);

    // Calcul de la zone active
    double area = cropped.cols * cropped.rows;

    // Détection du passage d'un véhicule
    if (countNonZero(cropped) > up * area && pixImg == 0)
    {
        compt++;
        pixImg = 1;
    }
    else if (countNonZero(cropped) < down * area)
    {
        pixImg = 0;
    }
}

// Constructeur de la classe Camera
Camera::Camera()
{
    m_fps = 30; // Initialisation du nombre de frames par seconde à 30
}

// Fonction pour ouvrir le flux vidéo à partir d'un nom de fichier ou d'un numéro de webcam
bool Camera::open(std::string filename)
{
    m_fileName = filename;
    std::istringstream iss(filename.c_str());
    int devid;
    bool isOpen;
    // Essaie d'ouvrir le flux vidéo
    if (!(iss >> devid))
    {
        isOpen = m_cap.open(filename.c_str());
    }
    else
    {
        isOpen = m_cap.open(devid);
    }

    // Si l'ouverture échoue, affiche un message d'erreur
    if (!isOpen)
    {
        std::cerr << "Unable to open video file." << std::endl;
        return false;
    }

    // Définit le framerate, si impossible à lire, le fixe à 30 fps
    m_fps = m_cap.get(cv::CAP_PROP_FPS);
    if (m_fps == 0)
        m_fps = 30;
    return true;
}

// Fonction pour lire le flux vidéo et détecter les véhicules
void Camera::play()
{
    // Création de la fenêtre principale
    namedWindow("Video", cv::WINDOW_AUTOSIZE);

    bool isReading = true;

    // Calcul du temps d'attente pour obtenir le framerate désiré
    int timeToWait = 1000 / m_fps;

    cv::Mat m_edges;
    cv::Mat m_frame_blurred;
    cv::Mat hsv_img;
    cv::Mat mask;
    cv::Mat gray;
    cv::Mat prev;

    vector<Vec4i> lines;

    bool detect = false;

    // Paramètres pour la détection de couleur
    int hmin = 25, smin = 25, vmin = 60, hmax = 110, smax = 240, vmax = 200;

    // Initialisation des compteurs de véhicules
    int pixImg1 = 0, pixImg2 = 0, pixImg3 = 0, pixImg4 = 0;

    int comptG = 0; // Compteur pour les véhicules à gauche
    int comptD = 0; // Compteur pour les véhicules à droite

    while (isReading)
    {
        // Lecture d'une frame du flux vidéo
        isReading = m_cap.read(m_frame);

        if (isReading)
        {
            // Traitement de l'image pour la détection
            cv::cvtColor(m_frame, gray, cv::COLOR_BGR2GRAY);
            if (!detect)
            {
                cv::GaussianBlur(m_frame, m_frame_blurred, cv::Size(3, 3), 7);
                cv::cvtColor(m_frame_blurred, hsv_img, cv::COLOR_BGR2HSV);
				
                // Définition de la plage de couleur jaune en HSV
                Scalar lower = Scalar(hmin, smin, vmin);
                Scalar upper = Scalar(hmax, smax, vmax);
                cv::inRange(hsv_img, lower, upper, mask);
                cv::Canny(mask, m_edges, 180, 200, 3);

                HoughLinesP(m_edges, lines, 1.5, CV_PI / 180, 138, 100, 100);
                if (cv::countNonZero(mask) > 100)
                {
                    detect = true;
                }
                prev = gray.clone();
            }
            // Suite du traitement pour la détection de mouvement et de véhicules
            Mat result;
            absdiff(gray, prev, result);    // Différence entre l'image actuelle et la précédente
            medianBlur(result, result, 11); // Filtrage des bruits
            cv::threshold(result, result, 25, 255, THRESH_BINARY);

            // Création d'un élément structurant pour l'érosion et la dilatation
            int morph_size = 2;
            Mat element = getStructuringElement(MORPH_RECT, Size(2 * morph_size + 1, 2 * morph_size + 1), Point(morph_size, morph_size));
            Mat erod, open;

            // Application de l'érosion et de la dilatation pour améliorer la détection
            erode(result, erod, element, Point(-1, -1), 1);
            dilate(erod, open, element, Point(-1, -1), 1);

            // Détection des contours pour identifier les véhicules
            vector<vector<Point>> contours;
            findContours(open, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
            for (size_t i = 0; i < lines.size(); i++)
            {
                Vec4i l = lines[i];
                drawInfiniteLine(m_frame, Point(l[0], l[1]), Point(l[2], l[3]), Scalar(255, 0, 0), 3);
            }
            for (int i = 0; i < contours.size(); i++)
            {
                if (contourArea(contours[i]) < 10)
                {
                    continue;
                }
                Scalar color = Scalar(0, 0, 255);
                Rect rect = boundingRect(contours[i]);
                rectangle(m_frame, rect, color, 2);
            }

            // Définition des zones d'intérêt pour le comptage des véhicules
            zone(90, 90, 80, 6, 45, m_frame, open, pixImg1, comptG, 0.4, 0.39);
            zone(150, 150, 80, 6, 45, m_frame, open, pixImg2, comptG, 0.4, 0.39);
            zone(600, 150, 100, 6, -25, m_frame, open, pixImg3, comptD, 0.4, 0.39);
            zone(720, 100, 80, 6, -25, m_frame, open, pixImg4, comptD, 0.4, 0.39);

            // Affichage du nombre de véhicules détectés à gauche et à droite
            string disp = "Voiture a gauche : " + std::to_string(comptG) + " || Voiture a droite : " + std::to_string(comptD);
            putText(m_frame, disp, Point(10, 20), FONT_HERSHEY_SIMPLEX, 0.75, Scalar(0, 0, 255), 2);

            // Affichage de l'image traitée
            imshow("Video", m_frame);
        }
        else
        {
            printf("\nOn a %d voiture à gauche et %d voiture à droite. Ce qui nous fait %d voitures qui passe\n", comptG, comptD, comptG + comptD);
            std::cerr << "Unable to read device" << std::endl;
        }

        // Si la touche échappe est pressée, arrêt du programme
        if (waitKey(timeToWait) % 256 == 27)
        {
            std::cerr << "Stopped by user" << std::endl;
            isReading = false;
        }
    }
}

// Fonction pour fermer le flux vidéo et les fenêtres d'affichage
bool Camera::close()
{
    // Fermeture du flux vidéo
    m_cap.release();

    // Fermeture de toutes les fenêtres
    destroyAllWindows();
    usleep(100000); // Attente pour s'assurer que tout est bien fermé
    return true;
}
