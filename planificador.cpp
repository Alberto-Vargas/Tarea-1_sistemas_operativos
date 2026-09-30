
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <queue>
#include <unordered_map>
#include <cstdlib>
#include <ctime>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <unistd.h>
#include <sys/wait.h>
using namespace std;

enum Estado { PENDIENTE, EJECUTANDO, FINALIZADA, FALLIDA, ABORTADA };

struct Actividad {
    string id;  // id de la actividad, texto
    string nombre; // nombres de actividades 
    int tiempo_ms; 
    vector<string> dependencias; // ids de las que tienen que terminar antes
    vector<int> dependientes;   // posiciones de las que esperan 
    int pendientes;            // cuantas dependencias le faltan por terminar
    Estado estado;
    vector<string> insumos;      
    int fd_lectura;              // pipe por donde el padre lee lo que dice el hijo
};
const int MAX_INSUMOS = 20;    // mensajes maximoss
const int MAX_MENSAJE = 60;   // y su largo maximo de cadea mensaje 
volatile sig_atomic_t sigint_recibido = 0;   // bandera del Control C
void manejador_sigint(int) {
    sigint_recibido = 1;
}

//grafo
// sacar espacions del principio al fin y si es todo espacios return vacio
string limpiar(const string& s) {
    size_t inicio = s.find_first_not_of(" \t\r\n");
    if (inicio == string::npos) return "";
    size_t fin = s.find_last_not_of(" \t\r\n");
    return s.substr(inicio, fin - inicio + 1);
}
// para abrir el archivo 
bool leer_plan(const string& archivo, vector<Actividad>& actividades) {
    ifstream entrada(archivo);
    if (!entrada.is_open()) {
        cerr << "No se pudo abrir " << archivo << endl;
        return false;
}

string linea;//leer lineas y saltar vacias 
while (getline(entrada, linea)) {
    if (limpiar(linea).empty()) continue; 

        // Separar la linea por el :
        vector<string> campos;
        stringstream ss(linea);
        string campo;
    while (getline(ss, campo, ':')) {
            campos.push_back(limpiar(campo));
        }
    if (campos.size() < 2) { //tiene q venir si o si id y nombree
            cerr << "Linea invalida: " << linea << endl;
            return false;
}

     Actividad a;
     a.id = campos[0];
     a.nombre = campos[1];

  if (campos.size() >= 3 && !campos[2].empty()) {   //si trae tiempo se usa si no uno alazar
            a.tiempo_ms = atoi(campos[2].c_str());
} else {
            a.tiempo_ms = 100 + rand() % 4901;
}

      
     if (campos.size() >= 4) { //dependencias
          
            stringstream sd(campos[3]);
            string dep;
            while (getline(sd, dep, ',')) {
                dep = limpiar(dep);
                if (!dep.empty()) a.dependencias.push_back(dep);
}
}

        a.pendientes = 0;
        a.estado = PENDIENTE;
        a.fd_lectura = -1;
        actividades.push_back(a);
}
 return true;
}

bool construir_grafo(vector<Actividad>& actividades) {
    
    unordered_map<string, int> posicion;
    for (size_t i = 0; i < actividades.size(); i++) {
        if (posicion.count(actividades[i].id)) {
            cerr << "id repetido: " << actividades[i].id << endl;
            return false;
        }
        posicion[actividades[i].id] = i;
    }

    for (size_t i = 0; i < actividades.size(); i++) {
        for (const string& dep : actividades[i].dependencias) {
            if (!posicion.count(dep)) {
                cerr << "La actividad " << actividades[i].id
                     << " depende de un id que no existe: " << dep << endl;
                return false;
            }
            int j = posicion[dep];
            actividades[j].dependientes.push_back(i); 
            actividades[i].pendientes++;             
}
}
    return true;
}

//hijos y tuberiaspipes


string leer_todo(int fd) {   //leer pipe hasta q el otro lado locierra 
    string resultado;
    char buf[256];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        resultado.append(buf, n);
}
    return resultado;
}


void trabajar(int tiempo_ms) {
    struct timespec t;
    t.tv_sec = tiempo_ms / 1000;
    t.tv_nsec = (tiempo_ms % 1000) * 1000000L;
    nanosleep(&t, nullptr); //proceso duerme para no gastar cpu
}


void proceso_hijo(const Actividad& a, int desde_padre, int hacia_padre) {
  
    signal(SIGINT, SIG_DFL);

   //leer mensajes del padre
    string insumos = leer_todo(desde_padre);
    close(desde_padre);
    for (char& c : insumos) {
        if (c == '\n') c = ' ';
}
    cout << "[INICIO] " << a.id << " " << a.nombre << " (" << a.tiempo_ms << " ms)";
    if (!insumos.empty()) cout << " | insumos: " << insumos;
    cout << endl;

//Se simula el trabajo sleep
    trabajar(a.tiempo_ms);

   
    if (a.nombre.rfind("falla", 0) == 0) {
        cout << "[FALLO] " << a.id << " " << a.nombre << endl;
        _exit(1);
    }

 
    string mensaje = (a.id + ":" + a.nombre + ":listo").substr(0, MAX_MENSAJE);
    if (write(hacia_padre, mensaje.c_str(), mensaje.size()) < 0) _exit(1);
    close(hacia_padre);

    cout << "[FIN] " << a.id << " " << a.nombre << endl;
    _exit(0);
}


bool lanzar(int i, vector<Actividad>& actividades, unordered_map<pid_t, int>& en_ejecucion) {
    int padre_a_hijo[2]; //pipes
    int hijo_a_padre[2];

    if (pipe(padre_a_hijo) < 0 || pipe(hijo_a_padre) < 0) {
        perror("pipe");
        return false;
    
}

    pid_t pid = fork();  //proceso hijo
    if (pid < 0) {
        perror("fork");
        return false;
}

    if (pid == 0) {
     //cierra los pipes de otro hijo
        close(padre_a_hijo[1]);
        close(hijo_a_padre[0]);
        for (auto& par : en_ejecucion) {
            close(actividades[par.second].fd_lectura);
}
    proceso_hijo(actividades[i], padre_a_hijo[0], hijo_a_padre[1]);
  }

 //padre cierra los extremos q no usa
close(padre_a_hijo[0]);
close(hijo_a_padre[1]);

  
    string texto;
    for (const string& m : actividades[i].insumos) {
        texto += m + "\n";
}
    if (!texto.empty() && write(padre_a_hijo[1], texto.c_str(), texto.size()) < 0) {
        perror("write");
}
    close(padre_a_hijo[1]);

    actividades[i].fd_lectura = hijo_a_padre[0];
    actividades[i].estado = EJECUTANDO;
    en_ejecucion[pid] = i;
    return true;
}

//control C 

//se recorre todo 
void abortar_rama(int origen, vector<Actividad>& actividades) {
    queue<int> cola;
    cola.push(origen);
    while (!cola.empty()) {
        int x = cola.front();
        cola.pop();
        for (int d : actividades[x].dependientes) {
            if (actividades[d].estado == PENDIENTE) {
                actividades[d].estado = ABORTADA;
                cola.push(d);
   }
  }
 }
}


void cancelar_todo(vector<Actividad>& actividades, unordered_map<pid_t, int>& en_ejecucion) {
    for (auto& par : en_ejecucion) {
        kill(par.first, SIGTERM);
    }
    for (auto& par : en_ejecucion) {
        int estado;
        while (waitpid(par.first, &estado, 0) < 0 && errno == EINTR) {
        }
        close(actividades[par.second].fd_lectura);
        actividades[par.second].estado = ABORTADA;
    }
    en_ejecucion.clear();
    for (Actividad& a : actividades) {
        if (a.estado == PENDIENTE) a.estado = ABORTADA;
 }
}

//MAIN 

int main(int argc, char* argv[]) {//archivo k
    if (argc != 3) {
        cerr << "Uso: ./planificador plan.txt K" << endl;
        return 1;
}
    srand(time(nullptr));//semilla tiempos al azar 
    int K = atoi(argv[2]); //k maximos en proceso alavez
    if (K <= 0) {
        cerr << "K debe ser mayor que 0" << endl;
        return 1;
}

    vector<Actividad> actividades;
    if (!leer_plan(argv[1], actividades)) return 1;
    if (!construir_grafo(actividades)) return 1;

  //configura control C para q waitpid se interrumpa 
    struct sigaction sa;
    sa.sa_handler = manejador_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);


    signal(SIGPIPE, SIG_IGN);

    //actividades en cola
    queue<int> listos;
    for (size_t i = 0; i < actividades.size(); i++) {
        if (actividades[i].pendientes == 0) listos.push(i);
    }

    unordered_map<pid_t, int> en_ejecucion;    //pid del hijo posicion de su actividad

    while (!sigint_recibido && (!listos.empty() || !en_ejecucion.empty())) {
      //sigo respetando los k que hay
        while ((int)en_ejecucion.size() < K && !listos.empty()) {
            int i = listos.front();
            listos.pop();
            if (!lanzar(i, actividades, en_ejecucion)) {
                actividades[i].estado = FALLIDA;
                abortar_rama(i, actividades);
            }
        }
        if (en_ejecucion.empty()) continue;

    // espero a q termine hijo el padre sleep para q no gaste cpu
        int estado;
        pid_t pid = waitpid(-1, &estado, 0);
        if (pid < 0) {
            if (errno == EINTR) continue;   //control C y reviso 
            perror("waitpid");
            break;
        }

        int i = en_ejecucion[pid]; //actividad del hijo q termino 
        en_ejecucion.erase(pid);

   
        string mensaje = leer_todo(actividades[i].fd_lectura);
        close(actividades[i].fd_lectura);
//WIFEXITED pregunta estado del hijo
        if (WIFEXITED(estado) && WEXITSTATUS(estado) == 0) {
            actividades[i].estado = FINALIZADA;
         //actividad termina bien padre guarda mnsj en una lsita y se lo pasas al hijoo
            for (int d : actividades[i].dependientes) {
                if (actividades[d].insumos.size() < (size_t)MAX_INSUMOS) {
                    actividades[d].insumos.push_back(mensaje);
                }
                actividades[d].pendientes--;
                if (actividades[d].pendientes == 0) listos.push(d);
            }
        } else {
            actividades[i].estado = FALLIDA;
            abortar_rama(i, actividades);
            cout << "[RAMA ABORTADA] fallo la actividad " << actividades[i].id << endl;
        }
    }

    if (sigint_recibido) {
        cout << endl << "[SEREMI] Inspeccion! Abortando todas las actividades." << endl;
        cancelar_todo(actividades, en_ejecucion);
    }

    int finalizadas = 0;
    for (const Actividad& a : actividades) {
        if (a.estado == FINALIZADA) finalizadas++;
    }
    cout << "Fin: " << finalizadas << " de " << actividades.size() << " actividades finalizadas" << endl;
    return 0;
}

