# Planificador Dieciochero Tarea 1 sistemas Operativos, Universidad Diego Portales

Integrante Alberto Vargas

## Descripción general 

El programa lee un plan de actividades " plan.txt " que forma un DAG y lo ejecuta respetando dependencias 
entre actividades y con un límite de concurrencia "K".  
Cada actividad se ejecuta en un proceso independiente creado con "fork",
La comunicación entre el padre " planificador" y los hijos "actividades" se hace solo mediante pipes.  
Cuando una actividad termina bien, el padre reenvía su mensaje a las actividades que dependen de ella como insumo,
sí una actividad falla se aborta la rama y si el usuario Presiona Ctrl+C se abortan todas las actividades 

## Compilación 

se uso" c++ y se utilizo lo siguiente entregado por el profesor: 
g++ -Wall -Wextra -std=c++17 planificador.cpp -o planificador -lpthread

./planificador plan.txt K "
plan.txt archivo con las actividades respectivas
K: máximo de procesos hijos vivos al mismo tiempo
se incluye -lpthread ya que el programa no usa hilos restricción del enunciado

## Estructura 

- Actividad: guarda el id, nombre, tiempo, la lista de dependencias (ids), la lista de dependientes (posiciones en el vector), un contador "pendientes", el "estado", los mensajes recibidos ("insumos") y el "fd_lectura" del pipe por donde el padre lee el resultado del hijo.
- Estado: PENDIENTE, EJECUTANDO, FINALIZADA, FALLIDA o ABORTADA.
- listos (`queue<int>`): actividades sin dependencias pendientes que esperan un cupo.
- en_ejecucion (unordered_map<pid_t, int>): relaciona el pid de cada hijo vivo con la posicion de su actividad. Su tamaño nunca supera K.
  
## Funciones implementadas 

- limpiar: Elimina espacios y saltos de línea al inicio y final de un texto.   
- leer_plan: Lee el archivo plan.txt, separa los campos y dependencias. Asigna un tiempo aleatorio si falta y rechaza líneas incompletas.  
- construir_grafo: Construye el grafo de actividades, detectando IDs repetidos y dependencias inexistentes.  
- leer_todo: Lee datos de un pipe hasta que se cierra.  
- trabajar: Simula el trabajo usando nanosleep sin consumir CPU.  
- proceso_hijo: El hijo recibe sus datos, realiza el trabajo y avisa al padre cuando termina. Si el nombre comienza con "falla", termina con error.  
- lanzar: Crea los pipes, hace fork y configura la comunicación entre padre e hijo.   
- abortar_rama: Si una actividad falla, marca como ABORTADAS todas sus actividades dependientes que aún estén pendientes.  
- cancelar_todo: Al presionar Ctrl+C, termina los hijos activos, espera a que finalicen y marca como abortadas las actividades incompletas.  
- manejador_sigint: Solo registra que se recibió Ctrl+C mediante una bandera.  
- main: Valida los argumentos, carga el plan, configura las señales y ejecuta la planificación. Al final muestra cuántas actividades terminaron.  

## decisión de diseño 

- Procesos con fork() en vez de hilos: el enunciado prohíbe los hilos, y cada actividad queda aislada en su propio proceso, por lo que un fallo no afecta al planificador ni a las demás.
- DAG con contador de dependencias: cada actividad tiene un contador "pendientes" y una lista de "dependientes". Al terminar una, se descuenta 1 a sus dependientes y las que llegan a 0 pasan a la cola "listos". Costo O(V+E), adecuado para 10.000 actividades.
- Control de concurrencia: solo se lanza un proceso mientras en_ejecucion.size() < K. Como únicamente el padre crea procesos, nunca hay más de K hijos vivos.
- Sin busy-waiting: el padre espera con waitpid() y los hijos simulan el trabajo con nanosleep(), por lo que no hay espera activa ni consumo innecesario de CPU.
- Sin race conditions: solo el padre modifica el estado del grafo, no hay memoria compartida y la comunicación es por pipes. El manejador de SIGINT solo modifica una variable volatile sig_atomic_t.
- Dos pipes por actividad: uno para enviar los insumos del padre al hijo y otro para el resultado del hijo al padre. Cada proceso cierra los extremos que no usa, para recibir EOF correctamente y no dejar descriptores abiertos.
- Mensajes acotados: máximo 20 insumos por actividad y 60 caracteres por mensaje, lo que mantiene los mensajes pequeños y facilita la comunicación por pipes.
- Aislamiento de errores: si un hijo termina con código distinto de 0, se marca FALLIDA y abortar_rama() marca como ABORTADA a sus descendientes pendientes. El resto del plan continúa y una actividad abortada nunca se lanza.
- Ctrl+C: el manejador se instala con sigaction() sin SA_RESTART para que waitpid() se interrumpa (EINTR) y el padre detecte la bandera. Luego cancelar_todo() envía SIGTERM a los hijos, los espera con waitpid() para evitar zombies y marca como ABORTADA lo que no terminó. SIGPIPE se ignora para que un hijo muerto no termine al padre.
- Errores del sistema: si pipe() o fork() fallan, se aborta la rama correspondiente sin cerrar todo el programa.
