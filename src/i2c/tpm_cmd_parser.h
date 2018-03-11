/*
 * TPM Command Parser
 * 
 * Author: Jeremy Boone, NCC Group
 * Date  : November 10, 2017
 */

#ifndef _TPM_CMD_PARSER_H_
#define _TPM_CMD_PARSER_H_


/* 
 *  Data direction associated with a byte array. This helps me keep track when printing commands
 *  and responses to the Serial console.
 */ 
typedef enum {
  HOST_2_INTERPOSER = 0,
  INTERPOSER_2_HOST,
  INTERPOSER_2_TPM,
  TPM_2_INTERPOSER,
} interposer_dir;

/*
 * The Linux kernel uses these names for the TPM registers. Each write operation from the master to
 * the slave will be prefixed with 1 byte that represents the TPM register where the lower most bits
 * are the register and the uppermost bits are the locality.
 */
typedef enum _tpm_register {
  TPM_ACCESS    = 0x0000,
  TPM_STS       = 0x0001,
  TPM_BURST_CNT = 0x0002,
  TPM_DATA_FIFO = 0x0005,
  TPM_DID_VID   = 0x0006,
  TPM_REG_NONE  = 0xFFFF // Just a place holder to simplify logic in pp_register()
} tpm_register;


/* 
 *  Track the previous register that was written to, because subsequent read request needs
 *  to know.
 *  Also useful to track the burst count because when we read from the FIFO, we read this
 *  many bytes.
 */
extern tpm_register prev_reg_write;
extern unsigned int prev_burst_count;
extern unsigned int prev_command;
extern bool get_header;

// --------------------------------------------------------------------------
// TPM Command Ordinals and Structures
// --------------------------------------------------------------------------

/*
 * The interposer is aware of these commands
 */
#define TPM_ORD_OIAP                    0x0A
#define TPM_ORD_OSAP                    0x0B
#define TPM_ORD_TakeOwnership           0x0D
#define TPM_ORD_Extend                  0x14
#define TPM_ORD_PcrRead                 0x15
#define TPM_ORD_GetRandom               0x46
#define TPM_ORD_SelfTest                0x50
#define TPM_ORD_ContinueSelfTest        0x53
#define TPM_ORD_OwnerClear              0x5B
#define TPM_ORD_ForceClear              0x5D
#define TPM_ORD_GetCapability           0x65
#define TPM_ORD_GetCapabilityOwner      0x66
#define TPM_ORD_OwnerSetDisable         0x6E
#define TPM_ORD_PhysicalEnable          0x6F
#define TPM_ORD_SetOwnerInstall         0x71
#define TPM_ORD_PhysicalSetDeactivated  0x72
#define TPM_ORD_SetOperatorAuth         0x74
#define TPM_ORD_ReadPubek               0x7C
#define TPM_ORD_OwnerReadInternalPub    0x81
#define TPM_ORD_Startup                 0x99
#define TPM_ORD_FlushSpecific           0xBA
#define TPM_ORD_NV_ReadValue            0xCF
#define TSC_ORD_PhysicalPresence        0x4000000A


// Ensure all command structures are packed.
#pragma pack(push, 1)

/* 
 *  This 10-byte header will prepend every command that is sent from the host to the TPM.
 */
typedef struct _tpm_input_header {
  unsigned short tag;
  unsigned int   len;
  unsigned int   ord;
} tpm_input_header;

/* 
 *  This 10-byte header will prepend every response that is sent from the TPM to the host.
 */
typedef struct _tpm_output_header {
  unsigned short tag;
  unsigned int   len;
  unsigned int   code;
} tpm_output_header;

/*********************************************************************
 *  PCR Read
 *********************************************************************/
typedef struct _tpm_pcr_read_req_body {
  unsigned int pcr_index;
} tpm_pcr_read_req_body;

typedef struct _tpm_pcr_read_resp_body {
  byte out_digest[20];
} tpm_pcr_read_resp_body;

/*********************************************************************
 *  PCR Extend
 *********************************************************************/
typedef struct _tpm_pcr_extend_req_body {
  unsigned int pcr_index;
  byte         in_digest[20];
} tpm_pcr_extend_req_body;

typedef struct _tpm_pcr_extend_resp_body {
  byte out_digest[20];
} tpm_pcr_extend_resp_body;

/********************************************************************* 
 *  Get Random
 *********************************************************************/
typedef struct _tpm_get_rnd_req_body {
  unsigned int num_bytes;
} tpm_get_rnd_req_body;

#pragma pack(push, 1)
typedef struct _tpm_get_rnd_resp_body {
  unsigned int random_bytes_size;
  byte         random_bytes[128];
} tpm_get_rnd_resp_body;
#pragma pack(pop)

/*********************************************************************
 *  Set Operator Auth
 *********************************************************************/
typedef struct _tpm_op_auth_req_body {
  byte           operator_auth[20];
} tpm_op_auth_req_body;


/********************************************************************* 
 *  OSAP
 *********************************************************************/
typedef struct _tpm_osap_req_body {
  unsigned short entity_type;
  unsigned int   entity_value;
  byte           nonce_odd_osap[20];
} tpm_osap_req_body;

typedef struct _tpm_osap_resp_body {
  unsigned int auth_handle;
  byte         nonce_even[20];
  byte         nonce_even_osap[20];
} tpm_osap_resp_body;

/********************************************************************* 
 *  OIAP
 *********************************************************************/
typedef struct _tpm_oiap_resp_body {
  unsigned int auth_handle;
  byte         nonce_even[20];
} tpm_oiap_resp_body;


/********************************************************************* 
 *  Get Capability
 *********************************************************************/
typedef struct _tpm_get_cap_req_body {
  unsigned int cap_area;
  unsigned int sub_cap_size;
  byte         sub_cap[];
} tpm_get_cap_req_body;

typedef struct _tpm_get_cap_resp_body {
  unsigned int resp_size;
  byte         resp[];
} tpm_get_cap_resp_body;

/********************************************************************* 
 *  Startup
 *********************************************************************/
 typedef struct _tpm_startup_req_body {
    unsigned short startup_type;
 } tpm_startup_req_body;

/********************************************************************* 
 *  Read Public Endorsement Key (PUBEK)
 *********************************************************************/
typedef struct _tpm_read_pubek_req_body {
  byte anti_replay[20];
} tpm_read_pubek_req_body;

// TODO: Add support for ReadPubEK response body... its a complex structure.

#pragma pack(pop)

// --------------------------------------------------------------------------
// Function  prototypes
// --------------------------------------------------------------------------

void parse_request( interposer_dir dir, byte* data, size_t num_bytes );
void parse_response( interposer_dir dir, byte* recvbuf, size_t num_bytes );
unsigned int predict_response_size( void );

#endif // _TPM_CMD_PARSER_H_

