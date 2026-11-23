#include <iostream>
#include <vector>
#include <queue>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

//==================== SHADERS ====================
const char* vertexShaderSource =                       // vertex shader: recibe la posicion 2D
"#version 330 core\n"
"layout (location = 0) in vec2 aPos;\n"
"void main() { gl_Position = vec4(aPos, 0.0, 1.0); }\0";

const char* fragmentShaderSource =                     // fragment shader: pinta con el color uColor
"#version 330 core\n"
"out vec4 FragColor;\n"
"uniform vec4 uColor;\n"
"void main() { FragColor = uColor; }\0";

void framebuffer_size_callback(GLFWwindow* window, int width, int height) { glViewport(0, 0, width, height); } // ajusta el viewport al cambiar el tamano de la ventana

//==================== ESTRUCTURAS ====================
struct Pixel { bool activo = false; ImVec4 color = ImVec4(1, 1, 1, 1); };   // una celda del lienzo
enum Tool { BRUSH, ERASER, FILL, LINE, CIRCLE, TRIANGLE, SQUARE };            // herramientas disponibles
struct IPoint { int x; int y; };                                              // punto entero (x = columna, y = fila)
struct DPoint { double x; double y; };                                        // punto real (para rotar sin perder precision)

struct Shape                                                                   // figura = tipo + vertices + color
{
    Tool type = LINE;                                                          // LINE: 2 vertices | CIRCLE: centro + borde | TRIANGLE: 3 | SQUARE: 4
    std::vector<DPoint> v;
    ImVec4 color = ImVec4(1, 1, 1, 1);
};

//==================== RELLENO ====================
bool MismoTipo(const Pixel& a, const Pixel& b)                                 // true si ambos estan vacios o tienen el mismo color
{
    if (a.activo != b.activo) return false;
    if (!a.activo) return true;
    return a.color.x == b.color.x && a.color.y == b.color.y && a.color.z == b.color.z && a.color.w == b.color.w;
}

void FloodFill(std::vector<std::vector<Pixel>>& canvas, int startRow, int startCol, ImVec4 color) // relleno por BFS
{
    int N = (int)canvas.size();
    if (startRow < 0 || startRow >= N || startCol < 0 || startCol >= N) return;

    Pixel objetivo = canvas[startRow][startCol];                               // lo que se va a reemplazar
    if (objetivo.activo && objetivo.color.x == color.x && objetivo.color.y == color.y &&
        objetivo.color.z == color.z && objetivo.color.w == color.w) return;    // ya tiene ese color: no hacer nada

    std::vector<std::vector<bool>> visited(N, std::vector<bool>(N, false));   // evita procesar una celda dos veces
    std::queue<std::pair<int, int>> q;
    q.push({ startRow, startCol });
    visited[startRow][startCol] = true;

    const int dr[4] = { 1, -1, 0, 0 };                                         // vecinos: arriba, abajo, derecha, izquierda
    const int dc[4] = { 0, 0, 1, -1 };

    while (!q.empty())
    {
        auto cur = q.front(); q.pop();
        int r = cur.first, c = cur.second;
        canvas[r][c].activo = true;                                            // pinta la celda
        canvas[r][c].color = color;

        for (int i = 0; i < 4; i++)
        {
            int nr = r + dr[i], nc = c + dc[i];
            if (nr < 0 || nr >= N || nc < 0 || nc >= N) continue;             // fuera del lienzo
            if (visited[nr][nc]) continue;                                     // ya visitada
            if (!MismoTipo(canvas[nr][nc], objetivo)) continue;                // no es del mismo tipo que la inicial
            visited[nr][nc] = true;
            q.push({ nr, nc });
        }
    }
}

//==================== BRESENHAM (PARAMETRO DE DECISION) ====================
void BresenhamLinea(int x0, int y0, int x1, int y1, std::vector<IPoint>& out)  // |m|<=1: p0=2dy-dx | |m|>1: se intercambian los ejes
{
    int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);                        // distancias absolutas
    int sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;                      // direccion de avance en cada eje
    int x = x0, y = y0;

    if (dx >= dy)                                                              // caso: avanza siempre en x
    {
        int p = 2 * dy - dx;                                                   // parametro de decision inicial
        for (int i = 0; i <= dx; i++)
        {
            out.push_back({ x, y });
            x += sx;
            if (p >= 0) { y += sy; p += 2 * (dy - dx); }                       // p >= 0: sube y, p += 2(dy-dx)
            else p += 2 * dy;                                                  // p < 0: mismo y, p += 2dy
        }
    }
    else                                                                       // caso: avanza siempre en y
    {
        int p = 2 * dx - dy;
        for (int i = 0; i <= dy; i++)
        {
            out.push_back({ x, y });
            y += sy;
            if (p >= 0) { x += sx; p += 2 * (dx - dy); }
            else p += 2 * dx;
        }
    }
}

//==================== CIRCULO PUNTO MEDIO (PARAMETRO DE DECISION) ====================
void CirculoPuntoMedio(int xc, int yc, int r, std::vector<IPoint>& out)       // p0 = 1 - r, se calcula 1 octante y se reflejan 8 puntos
{
    int x = 0, y = r, p = 1 - r;
    while (x <= y)
    {
        out.push_back({ xc + x, yc + y }); out.push_back({ xc - x, yc + y });  // simetria de los 8 octantes
        out.push_back({ xc + x, yc - y }); out.push_back({ xc - x, yc - y });
        out.push_back({ xc + y, yc + x }); out.push_back({ xc - y, yc + x });
        out.push_back({ xc + y, yc - x }); out.push_back({ xc - y, yc - x });

        x++;
        if (p < 0) p += 2 * x + 1;                                             // p < 0: mismo y
        else { y--; p += 2 * (x - y) + 1; }                                    // p >= 0: baja y
    }
}

//==================== CREAR FIGURA A PARTIR DEL ARRASTRE ====================
Shape CrearFigura(Tool type, int startCol, int startRow, int endCol, int endRow, ImVec4 color) // convierte el arrastre en vertices
{
    Shape s; s.type = type; s.color = color;
    double x0 = startCol, y0 = startRow, x1 = endCol, y1 = endRow;

    if (type == LINE || type == CIRCLE)                                        // linea: inicio y fin | circulo: centro y punto del borde
    {
        s.v.push_back({ x0, y0 });
        s.v.push_back({ x1, y1 });
    }
    else if (type == TRIANGLE)                                                 // isosceles: base en la fila final, punta centrada en la fila inicial
    {
        s.v.push_back({ x0, y1 });
        s.v.push_back({ x1, y1 });
        s.v.push_back({ (x0 + x1) / 2.0, y0 });
    }
    else if (type == SQUARE)                                                   // esquina inicial + lado = max(|dx|, |dy|)
    {
        double dx = x1 - x0, dy = y1 - y0;
        double lado = std::max(std::abs(dx), std::abs(dy));
        double sgx = (dx >= 0) ? 1.0 : -1.0, sgy = (dy >= 0) ? 1.0 : -1.0;    // hacia donde se arrastro
        s.v.push_back({ x0, y0 });
        s.v.push_back({ x0 + sgx * lado, y0 });
        s.v.push_back({ x0 + sgx * lado, y0 + sgy * lado });
        s.v.push_back({ x0, y0 + sgy * lado });
    }
    return s;
}

//==================== TRASLACION ====================
void MoverFigura(Shape& s, double dx, double dy)                               // suma (dx, dy) a todos los vertices (dy > 0 = arriba)
{
    for (DPoint& p : s.v) { p.x += dx; p.y += dy; }
}

//==================== ROTACION CON SENO Y COSENO ====================
void RotarPunto(double& px, double& py, double cx, double cy, double rad)      // x' = cx + dx*cos - dy*sin | y' = cy + dx*sin + dy*cos
{
    double cs = std::cos(rad), sn = std::sin(rad);
    double dx = px - cx, dy = py - cy;                                         // posicion relativa al pivote
    px = cx + dx * cs - dy * sn;
    py = cy + dx * sn + dy * cs;
}

void RotarFigura(Shape& s, double gradosDelta)                                 // rota sobre su propio centro (+ = izquierda/antihorario, - = derecha)
{
    if (s.v.empty()) return;
    const double PI = 3.14159265358979323846;
    double rad = gradosDelta * PI / 180.0;                                     // grados a radianes
    double cx = 0.0, cy = 0.0;

    if (s.type == CIRCLE) { cx = s.v[0].x; cy = s.v[0].y; }                    // circulo: su centro es el primer punto
    else                                                                       // resto: promedio de los vertices
    {
        for (const DPoint& p : s.v) { cx += p.x; cy += p.y; }
        cx /= (double)s.v.size();
        cy /= (double)s.v.size();
    }
    for (DPoint& p : s.v) RotarPunto(p.x, p.y, cx, cy, rad);
}

//==================== PUNTOS DE UNA FIGURA ====================
void ObtenerPuntosFigura(const Shape& s, std::vector<IPoint>& out)             // aqui recien se redondea a celdas y se corre el algoritmo
{
    out.clear();
    if (s.v.empty()) return;

    if (s.type == LINE)                                                        // linea: Bresenham entre los 2 vertices
    {
        BresenhamLinea((int)std::lround(s.v[0].x), (int)std::lround(s.v[0].y),
            (int)std::lround(s.v[1].x), (int)std::lround(s.v[1].y), out);
    }
    else if (s.type == CIRCLE)                                                 // circulo: radio = distancia centro-borde
    {
        double ddx = s.v[1].x - s.v[0].x, ddy = s.v[1].y - s.v[0].y;
        int radio = (int)std::lround(std::sqrt(ddx * ddx + ddy * ddy));
        CirculoPuntoMedio((int)std::lround(s.v[0].x), (int)std::lround(s.v[0].y), radio, out);
    }
    else                                                                       // triangulo / cuadrado: una linea de Bresenham por lado
    {
        int n = (int)s.v.size();
        for (int i = 0; i < n; i++)
        {
            const DPoint& a = s.v[i];
            const DPoint& b = s.v[(i + 1) % n];                                // el ultimo vertice se une con el primero
            BresenhamLinea((int)std::lround(a.x), (int)std::lround(a.y),
                (int)std::lround(b.x), (int)std::lround(b.y), out);
        }
    }
}

//==================== MAIN ====================
int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);                             // OpenGL 3.3 core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 800, "Mini Paint", NULL, NULL);
    if (window == NULL) { std::cout << "Error creando ventana\n"; glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    if (!gladLoadGL()) { std::cout << "Error cargando GLAD\n"; return -1; }
    glViewport(0, 0, 800, 800);

    //==================== IMGUI ====================
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    //==================== SHADERS ====================
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);                    // compila el vertex shader
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);                // compila el fragment shader
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    GLuint shaderProgram = glCreateProgram();                                  // une ambos en un programa
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint colorLoc = glGetUniformLocation(shaderProgram, "uColor");            // ubicacion del uniform de color

    //==================== GRID ====================
    const int N = 20;                                                          // lienzo de N x N celdas
    float step = 2.0f / N;                                                     // tamano de una celda en coordenadas OpenGL (-1 a 1)

    std::vector<float> gridVertices;
    for (int i = 0; i <= N; i++)                                               // lineas verticales
    {
        float x = -1.0f + i * step;
        gridVertices.push_back(x); gridVertices.push_back(-1.0f);
        gridVertices.push_back(x); gridVertices.push_back(1.0f);
    }
    for (int i = 0; i <= N; i++)                                               // lineas horizontales
    {
        float y = -1.0f + i * step;
        gridVertices.push_back(-1.0f); gridVertices.push_back(y);
        gridVertices.push_back(1.0f);  gridVertices.push_back(y);
    }

    GLuint gridVAO, gridVBO;
    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    //==================== PIXELS ====================
    GLuint pixelVAO, pixelVBO;                                                 // buffer reutilizado para dibujar cada celda
    glGenVertexArrays(1, &pixelVAO);
    glGenBuffers(1, &pixelVBO);

    std::vector<std::vector<Pixel>> canvas(N, std::vector<Pixel>(N));          // el lienzo
    Tool currentTool = BRUSH;                                                  // herramienta activa
    ImVec4 selectedColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);                     // color actual (rojo)

    bool wasLeftDown = false;                                                  // estado del clic en el frame anterior
    bool drawing = false;                                                      // true mientras se arrastra una figura
    int startRow = 0, startCol = 0, endRow = 0, endCol = 0;                    // inicio y fin del arrastre

    bool hasPending = false;                                                   // hay una figura pendiente (rotable/movible)
    Shape pending;
    float pasoRotacion = 15.0f;                                                // grados por clic
    double anguloTotal = 0.0;                                                  // giro acumulado (solo para mostrar)
    int pasoMover = 1;                                                         // celdas por clic

    std::vector<IPoint> puntos;                                                // puntos calculados de la figura

    auto drawCell = [&](int row, int col, const ImVec4& color)                 // dibuja una celda con un color
    {
        float x1 = -1.0f + col * step, y1 = -1.0f + row * step;
        float x2 = x1 + step, y2 = y1 + step;
        float quad[] = { x1, y1, x2, y1, x2, y2,   x1, y1, x2, y2, x1, y2 };   // 2 triangulos = 1 cuadrado

        glBindVertexArray(pixelVAO);
        glBindBuffer(GL_ARRAY_BUFFER, pixelVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glUniform4f(colorLoc, color.x, color.y, color.z, 1.0f);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    };

    auto commitPending = [&]()                                                 // aplica la figura pendiente al lienzo
    {
        if (!hasPending) return;

        std::vector<IPoint> pts;
        ObtenerPuntosFigura(pending, pts);
        for (const IPoint& p : pts)
        {
            if (p.x >= 0 && p.x < N && p.y >= 0 && p.y < N)                    // solo celdas dentro del lienzo
            {
                canvas[p.y][p.x].activo = true;
                canvas[p.y][p.x].color = pending.color;
            }
        }
        hasPending = false;
        anguloTotal = 0.0;
    };

    //==================== LOOP ====================
    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);                                  // fondo gris oscuro
        glClear(GL_COLOR_BUFFER_BIT);

        //==================== IMGUI ====================
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Herramientas", NULL, ImGuiWindowFlags_AlwaysAutoResize);

        if (ImGui::Button("Pincel")) { commitPending(); currentTool = BRUSH; }    // cambiar de herramienta aplica la figura pendiente
        if (ImGui::Button("Borrador")) { commitPending(); currentTool = ERASER; }
        if (ImGui::Button("Relleno")) { commitPending(); currentTool = FILL; }
        if (ImGui::Button("Linea (Bresenham)")) { commitPending(); currentTool = LINE; }
        if (ImGui::Button("Circulo (Punto medio)")) { commitPending(); currentTool = CIRCLE; }
        if (ImGui::Button("Triangulo")) { commitPending(); currentTool = TRIANGLE; }
        if (ImGui::Button("Cuadrado")) { commitPending(); currentTool = SQUARE; }

        const char* nombres[] = { "Pincel", "Borrador", "Relleno", "Linea", "Circulo", "Triangulo", "Cuadrado" };
        ImGui::Text("Herramienta: %s", nombres[currentTool]);
        ImGui::Separator();
        ImGui::ColorEdit4("Color", (float*)&selectedColor);

        if (hasPending)                                                        // panel de la figura pendiente
        {
            ImGui::Separator();
            ImGui::Text("Figura pendiente");

            ImGui::SliderFloat("Paso", &pasoRotacion, 1.0f, 90.0f, "%.0f grados");               // grados por clic
            if (ImGui::Button("<- Rotar izquierda")) { RotarFigura(pending, +pasoRotacion); anguloTotal += pasoRotacion; }
            ImGui::SameLine();
            if (ImGui::Button("Rotar derecha ->")) { RotarFigura(pending, -pasoRotacion); anguloTotal -= pasoRotacion; }
            ImGui::Text("Giro acumulado: %.0f grados", anguloTotal);

            ImGui::Separator();
            ImGui::SliderInt("Celdas por clic", &pasoMover, 1, 5);                                // celdas por clic al mover
            if (ImGui::Button("Arriba"))    MoverFigura(pending, 0.0, +pasoMover);
            if (ImGui::Button("Izquierda")) MoverFigura(pending, -pasoMover, 0.0);
            ImGui::SameLine();
            if (ImGui::Button("Derecha"))   MoverFigura(pending, +pasoMover, 0.0);
            if (ImGui::Button("Abajo"))     MoverFigura(pending, 0.0, -pasoMover);

            ImGui::Separator();
            if (ImGui::Button("Aplicar")) commitPending();                                        // dibuja la figura en el lienzo
            ImGui::SameLine();
            if (ImGui::Button("Cancelar")) { hasPending = false; anguloTotal = 0.0; }             // descarta la figura
        }

        ImGui::End();

        //==================== INPUT ====================
        bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        bool overUI = ImGui::GetIO().WantCaptureMouse;                         // true si el mouse esta sobre el panel de ImGui

        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        int width, height;
        glfwGetWindowSize(window, &width, &height);

        if (width > 0 && height > 0)
        {
            bool inside = mouseX >= 0 && mouseX < width&& mouseY >= 0 && mouseY < height;   // mouse dentro de la ventana

            int col = (int)((mouseX / width) * N);                             // pixel de pantalla a celda
            int row = (int)(((height - mouseY) / height) * N);                 // la fila 0 esta abajo
            int colClamp = std::max(0, std::min(N - 1, col));                  // limita al lienzo (permite arrastrar fuera)
            int rowClamp = std::max(0, std::min(N - 1, row));

            bool justPressed = leftDown && !wasLeftDown && !overUI;            // el clic empezo en este frame
            bool justReleased = !leftDown && wasLeftDown;                      // el clic se solto en este frame
            bool esFigura = currentTool == LINE || currentTool == CIRCLE || currentTool == TRIANGLE || currentTool == SQUARE;

            if (justPressed && inside)                                         // ---------- clic inicial ----------
            {
                commitPending();                                               // un clic nuevo aplica la figura pendiente
                if (esFigura) { drawing = true; startRow = endRow = rowClamp; startCol = endCol = colClamp; }
                else if (currentTool == FILL) FloodFill(canvas, rowClamp, colClamp, selectedColor);  // el relleno se aplica una vez por clic
            }

            if (leftDown && !overUI && inside)                                 // ---------- mantener presionado ----------
            {
                if (currentTool == BRUSH) { canvas[rowClamp][colClamp].activo = true; canvas[rowClamp][colClamp].color = selectedColor; }
                else if (currentTool == ERASER) canvas[rowClamp][colClamp].activo = false;
            }

            if (drawing && leftDown) { endRow = rowClamp; endCol = colClamp; } // actualiza el punto final mientras se arrastra

            if (drawing && justReleased)                                       // ---------- soltar ----------
            {
                drawing = false;
                pending = CrearFigura(currentTool, startCol, startRow, endCol, endRow, selectedColor);   // la figura queda pendiente
                hasPending = true;
                anguloTotal = 0.0;
            }
        }
        wasLeftDown = leftDown;

        //==================== DRAW ====================
        glUseProgram(shaderProgram);

        for (int row = 0; row < N; row++)                                      // celdas pintadas del lienzo
            for (int col = 0; col < N; col++)
                if (canvas[row][col].activo) drawCell(row, col, canvas[row][col].color);

        if (drawing)                                                           // vista previa mientras se arrastra
        {
            Shape preview = CrearFigura(currentTool, startCol, startRow, endCol, endRow, selectedColor);
            ObtenerPuntosFigura(preview, puntos);
            for (const IPoint& p : puntos)
                if (p.x >= 0 && p.x < N && p.y >= 0 && p.y < N) drawCell(p.y, p.x, preview.color);
        }

        if (hasPending)                                                        // figura pendiente con su rotacion y posicion actuales
        {
            ObtenerPuntosFigura(pending, puntos);
            for (const IPoint& p : puntos)
                if (p.x >= 0 && p.x < N && p.y >= 0 && p.y < N) drawCell(p.y, p.x, pending.color);
        }

        glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);                         // grilla en blanco
        glBindVertexArray(gridVAO);
        glDrawArrays(GL_LINES, 0, (GLsizei)(gridVertices.size() / 2));
        glBindVertexArray(0);

        //==================== IMGUI RENDER ====================
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    //==================== CLEANUP ====================
    glDeleteVertexArrays(1, &gridVAO);  glDeleteBuffers(1, &gridVBO);
    glDeleteVertexArrays(1, &pixelVAO); glDeleteBuffers(1, &pixelVBO);
    glDeleteProgram(shaderProgram);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}