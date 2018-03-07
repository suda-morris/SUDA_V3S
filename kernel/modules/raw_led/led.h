#pragma once

#define LED_MAGIC 'l'

#define LED_ON      _IOW(LED_MAGIC,0,unsigned int)
#define LED_OFF     _IOW(LED_MAGIC,1,unsigned int)

#define LED_RED     0
#define LED_GREEN   1
#define LED_BLUE    2
