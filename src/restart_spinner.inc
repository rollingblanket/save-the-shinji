// Hold-to-restart spinner: a solid ring at screen center that fills gold
// clockwise from 12 o'clock as SPACE is held, drawn on the low-res grid
// with a dark backdrop track and outline so it reads against any scene.
#define RESTART_SPINNER_PI 3.14159265358979323846f

static void DrawRestartSpinner(float progress)
{
    int cx = RIVER_RES/2;
    int cy = RIVER_RES/2;
    float filled = progress*2.0f*RESTART_SPINNER_PI;

    for (int by = cy - 9; by <= cy + 9; by++)
    {
        for (int bx = cx - 9; bx <= cx + 9; bx++)
        {
            float dx = (float)(bx - cx);
            float dy = (float)(by - cy);
            float dist = sqrtf(dx*dx + dy*dy);
            if ((dist < 4.0f) || (dist > 8.5f)) continue;

            Color c;
            if ((dist < 4.8f) || (dist > 7.7f)) c = (Color){ 13, 15, 18, 255 };  // Outline
            else
            {
                float angle = atan2f(dx, -dy);      // 0 at 12 o'clock, clockwise
                if (angle < 0.0f) angle += 2.0f*RESTART_SPINNER_PI;
                c = (angle <= filled)? (Color){ 240, 224, 138, 255 } : (Color){ 45, 50, 60, 255 };
            }
            DrawRectangle((int)(bx*RIVER_PIXEL), (int)(by*RIVER_PIXEL),
                          (int)RIVER_PIXEL, (int)RIVER_PIXEL, c);
        }
    }
}

#undef RESTART_SPINNER_PI
