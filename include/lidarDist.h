# ifndef LIDARDIST_H
# define LIDARDIST_H
# include <freertos/FreeRTOS.h>
# include <freertos/task.h>
# include <freertos/semphr.h>

void lidarDist_task(void *pvParameters);
int distanceFast(bool biasCorrection);

#endif