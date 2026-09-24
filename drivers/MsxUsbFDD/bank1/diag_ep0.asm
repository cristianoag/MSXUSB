; MSXUSB USB FDD BIOS
; Based on the Rookie Drive USB FDD BIOS created by Konamiman in 2018
; This version is adapted to work with the latest version of the MSXUSB project
;
; This file contains endpoint 0 transfer experiments, used to find out
; why some devices (e.g. TEAC FD-05PUB, which has an endpoint 0 max packet size
; of 8 bytes) don't deliver the second packet of a control transfer data stage.
; They run when the device initialization fails when getting descriptors
; (only if EP0_DIAGNOSTICS=1), and print one result line per experiment:
;
; DD: First 8 bytes of the device descriptor, at address 0
; T0: GET_DESCRIPTOR(18): SETUP, IN DATA1, IN DATA0 (the normal sequence)
; T1: Same, but expecting DATA1 again in the second IN
; D5: Same as T0, with a 5ms delay before the second IN
; BI: CH376 built-in GET_DESCR for the device descriptor (B2: configuration descriptor)
; SC: SET_CONFIGURATION(1): SETUP, status IN
; AD: CBI ADSC with a 12 byte TEST UNIT READY command:
;     SETUP, OUT 8 bytes, OUT 4 bytes, status IN
;
; For each stage the raw CH376 status is printed, and for IN stages
; also the number of bytes received ("status/count"). 14 means success,
; 2A is NAK, 2E is STALL, 20 is timeout.

    if EP0_DIAGNOSTICS = 1

DIAG_EP0_BUF_SIZE: equ 72

DIAG_EP0:
    ld ix,-DIAG_EP0_BUF_SIZE
    add ix,sp
    ld sp,ix

    ;--- Enumerate the device again until SET_ADDRESS,
    ;    it's the same sequence that USB_INIT_DEV follows

    call HW_BUS_RESET
    push ix
    push ix
    pop de
    ld hl,USB_CMD_GET_DEV_DESC_64
    xor a
    ld b,64
    call HW_CONTROL_TRANSFER
    pop ix

    ld hl,DG_DD_S
    call PRINT
    push ix
    pop hl
    ld b,8
_DIAG_EP0_DD_LOOP:
    ld a,(hl)
    call PRINT_HEX
    ld a,' '
    call CHPUT
    inc hl
    djnz _DIAG_EP0_DD_LOOP
    ld hl,CRLF_S
    call PRINT

    call HW_BUS_RESET
    push ix
    ld hl,USB_CMD_SET_ADDRESS
    ld de,0
    xor a
    ld b,8
    call HW_CONTROL_TRANSFER
    pop ix
    ld bc,100
    call CH_DELAY

    ld a,3Fh    ;Don't retry NAKs (retries are done by software, max 300ms)
    call CH_SET_RETRY_VALUE
    ld a,USB_DEVICE_ADDRESS
    call CH_SET_TARGET_DEVICE_ADDRESS

    ;--- Get device descriptor experiments

    ld hl,DG_T0_S
    xor a
    ld c,0
    call _DG_GET18

    ld hl,DG_T1_S
    ld a,80h
    ld c,0
    call _DG_GET18

    ld hl,DG_D5_S
    xor a
    ld c,50
    call _DG_GET18

    ;--- CH376 built-in commands

    ld hl,DG_BI_S
    ld a,1
    call _DG_BUILTIN
    ld hl,DG_B2_S
    ld a,2
    call _DG_BUILTIN
    ld hl,CRLF_S
    call PRINT

    ;--- SET_CONFIGURATION, then a CBI command

    ld hl,DG_SC_S
    call PRINT
    ld hl,DG_CMD_SET_CONFIG
    call _DG_SETUP
    ld a,80h
    call _DG_IN

    ld hl,DG_AD_S
    call PRINT
    ld hl,DG_CMD_ADSC
    call _DG_SETUP
    ld a,40h
    ld b,8
    call _DG_OUT
    xor a
    ld b,4
    call _DG_OUT
    ld a,80h
    call _DG_IN
    ld hl,CRLF_S
    call PRINT

    ;--- Done, restore the NAK retry mode

    or a
    call HW_CONFIGURE_NAK_RETRY

    ld ix,DIAG_EP0_BUF_SIZE
    add ix,sp
    ld sp,ix
    ret


    ;--- GET_DESCRIPTOR(18) experiment
    ;    Input: HL = Label, A = toggle for the second IN, C = delay before the second IN
    ;           (in units of 0.1ms, 0 = no delay)

_DG_GET18:
    push af
    push bc
    call PRINT
    ld hl,USB_CMD_GET_DEV_DESC_18
    call _DG_SETUP
    ld a,80h
    call _DG_IN
    pop bc
    ld a,c
    or a
    jr z,_DG_GET18_2
    ld b,0
    call CH_DELAY
_DG_GET18_2:
    pop af
    call _DG_IN
    ld hl,CRLF_S
    jp PRINT


    ;--- Execute a SETUP transaction and print the status
    ;    Input: HL = Setup packet

_DG_SETUP:
    ld b,8
    call CH_WRITE_DATA
    xor a
    ld e,0
    ld b,CH_PID_SETUP
    call CH_ISSUE_TOKEN
    call CH_WAIT_INT_AND_GET_RESULT
    jr _DG_PRINT_STATUS_SPACE


    ;--- Execute an IN transaction on endpoint 0, print "status/count"
    ;    Input: A = Toggle in bit 7

_DG_IN:
    ld hl,300
    ld (CH_SW_NAK_LEFT),hl
    ld e,0
    call _CH_IN_TOKEN
    push af
    ld a,(CH_LAST_STATUS)
    call PRINT_HEX
    ld a,'/'
    call CHPUT
    pop af
    ld b,0
    or a
    jr nz,_DG_IN_2
    push ix
    pop hl
    call CH_READ_DATA
_DG_IN_2:
    ld a,b
    call PRINT_HEX
    ld a,' '
    jp CHPUT


    ;--- Execute an OUT transaction on endpoint 0 with zeros as data, print the status
    ;    Input: A = Toggle in bit 6, B = Length

_DG_OUT:
    ld hl,300
    ld (CH_SW_NAK_LEFT),hl
    ld hl,DG_ZEROS
    ld e,0
    call _CH_OUT_PACKET
_DG_PRINT_STATUS_SPACE:
    ld a,(CH_LAST_STATUS)
    call PRINT_HEX
    ld a,' '
    jp CHPUT


    ;--- Execute a CH376 built-in GET_DESCR, print "status/count"
    ;    Input: HL = Label, A = Descriptor type

_DG_BUILTIN:
    push af
    call PRINT
    ld a,0FFh   ;The CH376 handles NAKs by itself here
    call CH_SET_RETRY_VALUE
    pop bc
    ld a,CH_CMD_GET_DESCR
    out (CH_COMMAND_PORT),a
    ld a,b
    out (CH_DATA_PORT),a
    call CH_WAIT_INT_AND_GET_RESULT
    push af
    ld a,(CH_LAST_STATUS)
    call PRINT_HEX
    ld a,'/'
    call CHPUT
    pop af
    ld b,0
    or a
    jr nz,_DG_BUILTIN_2
    push ix
    pop hl
    call CH_READ_DATA
_DG_BUILTIN_2:
    ld a,b
    call PRINT_HEX
    ld a,' '
    call CHPUT
    ld a,3Fh
    jp CH_SET_RETRY_VALUE


DG_CMD_SET_CONFIG:
    db 0, 9, 1, 0, 0, 0, 0, 0

DG_CMD_ADSC:
    db 21h, 0, 0, 0, 0, 0, 12, 0

DG_ZEROS:
    db 0, 0, 0, 0, 0, 0, 0, 0

DG_DD_S: db "DD:",0
DG_T0_S: db "T0:",0
DG_T1_S: db "T1:",0
DG_D5_S: db "D5:",0
DG_BI_S: db "BI:",0
DG_B2_S: db "B2:",0
DG_SC_S: db "SC:",0
DG_AD_S: db " AD:",0

    endif
