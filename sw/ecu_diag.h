#ifndef ECU_DIAG_H
#define ECU_DIAG_H

uint8_t ecuDiag_kw();

typedef enum 
{
  DIAG_MODE_AFR_BOOST,
  DIAG_MODE_DIAG,
  DIAG_MODE_IGNITION,
  DIAG_MODE_AFR_GRAPH,
  DIAG_MODE_LAMBDA_GRAPH,
  DIAG_MODE_LAMBDA_BOOST,
  DIAG_MODE_LOGGER,
  DIAG_MODE_END
} diagMode_t;


#endif
