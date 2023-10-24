#include <iostream>
#include <string>
#include <fstream>
#include <omp.h>
#include <time.h>
#include <cmath>

using namespace std;

float distanciaEuclidiana(float x1, float y1, float x2, float y2) { // Usamos distancia euclidiana por la presentación de clase
    float x = x1 - x2;
    float y = y1 - y2;
    return std::sqrt(x * x + y * y);
}

void noise_detection(float** points, float epsilon, int min_samples, long long int size) {
    int* num_cluster = new int[size]; // En esta línea inicializamos el array que le va a pegar a los puntos su respectivo número de cluster. Ej 4.3, 7.6, -1 si es ruido


    for (long long int i = 0; i < size; i++) {
        num_cluster[i] = 0;  // Los ponemos todos en 0, que significa que no están visitados aún 
    }

    int cluster = 1;  // Inicializamos los números de cluster

    for (long long int i = 0; i < size; i++) {
        if (num_cluster[i] != 0) {
            continue;  // Evitamos volver a visitar nodos que no tengan asignación a cluster - NOTA: no necesariamente que ya hayan sido visitados porque un edge se puede volver un core
        }

        int tam = -1;  // Inicializamos el tamaño del array de los vecinos del punto a analizar
        int* vecinos = new int[size]; // Inicializammos el array de los vecinos

        for (long long int j = 0; j < size; ++j) {
            if (i != j && distanciaEuclidiana(points[i][0], points[i][1], points[j][0], points[j][1]) <= epsilon) { // Evitamos que el punto se compare con el mismo y hacemos que la distancia sea menor a epsilon
                vecinos[++tam] = j;  // Guardamos el índice de los vecinos en el arreglo. Pero lo haccemos con ++tamm porque queremmos sumar antes. Tam está inicializado en -1.
            }
        }

        if (tam < min_samples) { // Si no hay suficientes puntos en el radio epsilon, es decir, si el numero de vecinos es mmenor al de min_samples, entoncces no es un punto core
            num_cluster[i] = -1;  // Ruido
        } else {
            num_cluster[i] = cluster;  // Si si tiene suficientes puntos en el radio epsilon, le asignamos el número del cluster
            for (int k = 0; k <= tam; k++) { // Iterammos sobre los vecinos
                long long int l = vecinos[k];
                if (num_cluster[l] == -1) {
                    num_cluster[l] = cluster;  // Si antes fue cclasificado commo ruido, le asignamos su cluster correspondiente
                } else if (num_cluster[l] == 0) {
                    num_cluster[l] = cluster;  // Si el vecino noo ha sido visitado, le asignamos su cluster
                }
            }
        }

        delete[] vecinos;  // Le quitamos memoria asignada a vecinos
    }

    for (long long int i = 0; i < size; i++) {
        points[i][2] = static_cast<float>(num_cluster[i]);  // Aqui solo le asignamos en una tercera columna el cluster correspondiente a los puntos, como en el código madre
    }

    delete[] num_cluster;  // Le quitamos memoria asignada a la asignación de nummeros de clusters a los puntos
}

void noise_detection_p(float** points, float epsilon, int min_samples, long long int size, long long int ncores) {
    
    int* num_cluster = new int[size]; 

    #pragma omp parallel for // Hacemos un for paralelo para llenar la asignacion con 0s. No nos importa que distintos hilos trabajan sobre lo mismo porque es llenarla con 0s
    for (long long int i = 0; i < size; i++) {
        num_cluster[i] = 0;  
    }

    int cluster = 1;  

    #pragma omp parallel for num_threads(ncores) //Hacemos un for paralelo con el nummero de cores correspondientes. 
    for (long long int i = 0; i < size; i++) {
        if (num_cluster[i] != 0) {
            continue;
        }

        int tam = -1;  
        int* vecinos = new int[size]; 

        for (long long int j = 0; j < size; ++j) {
            if (i != j && distanciaEuclidiana(points[i][0], points[i][1], points[j][0], points[j][1]) <= epsilon) {  
                vecinos[++tam] = j;  
            }
        }

        if (tam < min_samples) {
            #pragma omp critical // Establecemos una direactiva crítica. Esto, porque es importante clasificar correctamente el ruido y asi evitamos coondiciones de ccarrera. Si entra al else, entonces nos aseguramos que no era ruido.
            {
                num_cluster[i] = -1;  
            }
        } else {
            num_cluster[i] = cluster;  
            for (int k = 0; k <= tam; k++) { 
                long long int l = vecinos[k];
                if (num_cluster[l] == -1) {
                    num_cluster[l] = cluster;  
                } else if (num_cluster[l] == 0) {
                    num_cluster[l] = cluster;  
                }
            }
        }

        delete[] vecinos;  
    }

    #pragma omp parallel for //Parlelizamos la asignación de puntos. 
    for (long long int i = 0; i < size; i++) {
        points[i][2] = static_cast<float>(num_cluster[i]);  
    }

    delete[] num_cluster;  
}



void load_CSV(string file_name, float** points, long long int size) {
    ifstream in(file_name);
    if (!in) {
        cerr << "Couldn't read file: " << file_name << "\n";
    }
    long long int point_number = 0;
    while (!in.eof() && (point_number < size)) {
        char* line = new char[12];
        streamsize row_size = 12;
        in.read(line, row_size);
        string row = line;

        points[point_number] = new float[3]{0.0, 0.0, 0.0};
        points[point_number][0] = stof(row.substr(0, 5));
        points[point_number][1] = stof(row.substr(6, 5));
        point_number++;
    }
}

void save_to_CSV(string file_name, float** points, long long int size) {
    fstream fout;
    fout.open(file_name, ios::out);
    for (long long int i = 0; i < size; i++) {
        fout << points[i][0] << ","
             << points[i][1] << ","
             << points[i][2] << "\n";
    }
}

int main(int argc, char** argv) {

    const float epsilon = 0.03;
    const int min_samples = 10;
    const int np[] = {20000,40000,80000,120000,140000,160000,180000,200000};
    const int num_threads[] = {1, omp_get_num_procs()/2, omp_get_num_procs(), omp_get_num_procs()*2};


    for(int i=0;i<8;i++){
        const long long int size = np[i];
        float** points = new float*[size];
        const float epsilon = 0.03;
        const int min_samples = 10;
        const string input_file_name = to_string(size)+"_data.csv";
        const string output_file_name = to_string(size)+"_results.csv";


        for(long long int i = 0; i < size; i++) {
                points[i] = new float[3]{0.0, 0.0, 0.0}; 
                // index 0: position x
                // index 1: position y 
                // index 2: 0 for noise point, 1 for core point
        }

        load_CSV(input_file_name, points, size);

        double tiempo_agregado = 0.0;

        for(int j=0; j<3;j++){
            double inicio_serial = omp_get_wtime();
            noise_detection(points, epsilon,min_samples,size);
            double final_serial = omp_get_wtime();
            double dif = final_serial - inicio_serial;
            tiempo_agregado += dif;
        }
        
        double tiempo_final = tiempo_agregado/3;

        save_to_CSV(output_file_name, points, size);

        for(long long int i = 0; i < size; i++) {
                delete[] points[i];
            }

        cout << "1" <<  "," << size <<  ","  << tiempo_final << " \n" ;
    }

    for(int i=0; i < 8; i++){
        for(int j=0; j<4; j++){
            const long long int size = np[i];
            const int cores_v = num_threads[j];
            const string input_file_name = to_string(size)+"_data.csv";
            const string output_file_name = to_string(size)+"_results.csv";
            float** points = new float*[size];
            const float epsilon = 0.03;
            const int min_samples = 10;


            for(long long int i = 0; i < size; i++) {
                points[i] = new float[3]{0.0, 0.0, 0.0}; 
                // index 0: position x
                // index 1: position y 
                // index 2: 0 for noise point, 1 for core point
            }
            

            double tiempo_ag = 0.0;

            for(int k=0; k<3; k++){
                double inicio_paralela = omp_get_wtime();
                noise_detection_p(points, epsilon, min_samples, size, cores_v);
                double final_paralela = omp_get_wtime();
                double diff = final_paralela - inicio_paralela;
                tiempo_ag += diff;
            }
            
            double tiempo_f = tiempo_ag/3;



            for(long long int m = 0; m < size; m++) {
                delete[] points[m];
            }

             cout << cores_v <<  ","  << size << "," << tiempo_f << " \n";



        }
    }

    return 0;
    
}
