# Mini Paint (C++ / OpenGL / Dear ImGui)
Andrés Felipe Garcia Camacho//////
6014398
/////link del video explicativo: (https://drive.google.com/file/d/1g-bBytXvr4KtuGme2g6-czPTDy-zZSak/view?usp=sharing)

Editor de dibujo sobre una cuadrícula de píxeles hecho en C++ con **OpenGL 3.3 (core)**, **GLFW**, **GLAD** y **Dear ImGui**. Además de las herramientas básicas de un paint, implementa a mano los algoritmos clásicos de gráficos por computador: **línea de Bresenham**, **círculo de punto medio** (ambos con parámetro de decisión), **relleno por inundación**, y **rotación y traslación** de figuras con senos y cosenos.

## Características

- Lienzo de **N x N celdas** (por defecto 20 x 20, configurable con la constante `N`).
- Herramientas:
  - **Pincel**: pinta una celda con el color seleccionado.
  - **Borrador**: borra una celda.
  - **Relleno**: rellena una región conectada (se aplica una sola vez por clic).
  - **Línea** (Bresenham con parámetro de decisión).
  - **Círculo** (punto medio con parámetro de decisión).
  - **Triángulo** (isósceles).
  - **Cuadrado**.
- Selector de color RGBA con `ImGui::ColorEdit4`.
- **Figura pendiente**: al soltar el mouse, las figuras no se dibujan de inmediato. Quedan pendientes para poder **rotarlas** y **moverlas** antes de aplicarlas.
- Vista previa en tiempo real mientras se arrastra.

## Cómo se usa

### Dibujar

| Herramienta | Cómo usarla |
|---|---|
| Pincel / Borrador | Mantén el clic izquierdo y pasa el mouse sobre las celdas. |
| Relleno | Un clic sobre la región que quieres rellenar. |
| Línea | Clic en el punto inicial, arrastra y suelta en el punto final. |
| Círculo | Clic en el **centro**, arrastra hasta el borde y suelta. La distancia es el radio. |
| Triángulo | Arrastra una caja. La base queda en la fila donde sueltas y la punta, centrada, en la fila donde empezaste. |
| Cuadrado | Clic en una esquina y arrastra hacia la opuesta. El lado es el mayor entre el desplazamiento horizontal y el vertical. |

### Figura pendiente

Al soltar el clic en una figura aparece el panel **Figura pendiente**:

- **Paso**: grados que gira cada clic (1° a 90°).
- **Rotar izquierda / Rotar derecha**: giran la figura sobre su propio centro (izquierda = antihorario, derecha = horario).
- **Giro acumulado**: muestra el ángulo total aplicado.
- **Celdas por clic**: cuántas celdas se mueve la figura por clic (1 a 5).
- **Arriba / Abajo / Izquierda / Derecha**: desplazan la figura.
- **Aplicar**: dibuja la figura en el lienzo.
- **Cancelar**: descarta la figura.

La figura también se aplica automáticamente si haces un clic nuevo sobre el lienzo o cambias de herramienta. Si la sacas del lienzo, al aplicarla solo se pintan las celdas que queden dentro.

## Algoritmos implementados

### Línea de Bresenham (parámetro de decisión)

Para pendiente con |m| <= 1 se avanza en `x` y se decide si `y` cambia:

```
p0 = 2*dy - dx
si p < 0:  siguiente punto (x+1, y),   p = p + 2*dy
si p >= 0: siguiente punto (x+1, y+1), p = p + 2*(dy - dx)
```

Para |m| > 1 se intercambian los papeles de `x` e `y`. Funciona en los ocho octantes usando el signo de avance en cada eje.

### Círculo de punto medio (parámetro de decisión)

Se calcula un solo octante y se reflejan los otros siete puntos por simetría:

```
p0 = 1 - r
si p < 0:  siguiente punto (x+1, y),   p = p + 2*x + 1
si p >= 0: siguiente punto (x+1, y-1), p = p + 2*(x - y) + 1
```

(con `x` e `y` ya actualizados).

### Relleno por inundación (flood fill)

Recorrido por **BFS** con una cola y una matriz `visited`. Cada celda se procesa una sola vez, por lo que no entra en bucles infinitos. Compara estado y color de la celda inicial, así que también rellena zonas ya pintadas, y no hace nada si la región ya tiene el color elegido.

### Rotación con seno y coseno

Cada vértice se rota alrededor del centro de la figura:

```
x' = cx + (x - cx)*cos(θ) - (y - cy)*sin(θ)
y' = cy + (x - cx)*sin(θ) + (y - cy)*cos(θ)
```

Los vértices se guardan como `double` y **solo se redondean a celdas al dibujar**, por eso los giros repetidos no acumulan error de redondeo. El centro es el promedio de los vértices (en el círculo, su centro).

### Traslación

Se suma `(dx, dy)` a todos los vértices de la figura.

### Polígonos

El triángulo y el cuadrado se guardan como listas de vértices y se dibujan trazando una línea de Bresenham por cada lado, uniendo el último vértice con el primero.

## Estructura del código

Todo está en un solo archivo `main.cpp`:

| Elemento | Descripción |
|---|---|
| Shaders | Vertex shader (posición 2D) y fragment shader (color uniforme `uColor`). |
| `Pixel`, `Tool`, `IPoint`, `DPoint`, `Shape` | Estructuras de datos: celda del lienzo, herramientas, puntos enteros y reales, y figura (tipo + vértices + color). |
| `MismoTipo`, `FloodFill` | Relleno por inundación. |
| `BresenhamLinea` | Línea con parámetro de decisión. |
| `CirculoPuntoMedio` | Círculo con parámetro de decisión. |
| `CrearFigura` | Convierte el arrastre del mouse en vértices. |
| `MoverFigura` | Traslación. |
| `RotarPunto`, `RotarFigura` | Rotación con seno y coseno. |
| `ObtenerPuntosFigura` | Redondea a celdas y ejecuta el algoritmo que corresponda. |
| `main` | Ventana, ImGui, grilla, lógica de entrada y ciclo de render. |

## Requisitos

- Compilador C++ con soporte de C++11 o superior.
- GPU y drivers con soporte de **OpenGL 3.3**.
- Librerías:
  - [GLFW](https://www.glfw.org/)
  - [GLAD](https://glad.dav1d.de/) (OpenGL 3.3 core)
  - [Dear ImGui](https://github.com/ocornut/imgui) con los backends `imgui_impl_glfw` e `imgui_impl_opengl3`

## Compilación

Agrega al proyecto `main.cpp`, `glad.c`, los archivos de ImGui (`imgui*.cpp`) y los dos backends. Ejemplo con `g++` en Linux (ajusta las rutas a tu estructura):

```bash
g++ main.cpp glad.c imgui/*.cpp \
    imgui/backends/imgui_impl_glfw.cpp \
    imgui/backends/imgui_impl_opengl3.cpp \
    -Iimgui -Iimgui/backends -Iinclude \
    -lglfw -ldl -lGL -o minipaint

./minipaint
```

En Visual Studio basta con agregar esos archivos al proyecto, configurar las rutas de include y enlazar `glfw3.lib` y `opengl32.lib`.

## Notas y limitaciones

- Con una cuadrícula de 20 x 20, una figura rotada con ángulos que no son múltiplos de 90° se ve irregular por el redondeo a celdas. Subir `N` (por ejemplo a 40 o 60) da resultados más suaves.
- Un círculo rotado sobre su propio centro se ve igual, porque la rotación solo mueve su centro y este no cambia.
- El triángulo y el cuadrado se dibujan solo como contorno.
- Las figuras ya aplicadas al lienzo no se pueden rotar ni mover, solo la figura pendiente.

## Posibles mejoras

- Rellenar automáticamente triángulo y cuadrado.
- Herramienta de selección para rotar y mover figuras ya aplicadas.
- Mover la figura pendiente con las flechas del teclado.
- Guardar el dibujo como imagen.
- Deshacer y rehacer.
