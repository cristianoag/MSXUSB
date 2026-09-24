; MSXUSB USB FDD BIOS
; Based on the Rookie Drive USB FDD BIOS created by Konamiman in 2018
; This version is adapted to work with the latest version of the MSXUSB project
;
; This routine resets the USB hardware, resets and initializes the device,
; and prints the device name or the appropriate error message.
; It is executed at boot time and by CALL USBRESET.

VERBOSE_RESET:
    push iy
    ld iy,-36
    add iy,sp
    ld sp,iy
    call _VERBOSE_RESET
    ld iy,36
    add iy,sp
    ld sp,iy
    pop iy
    ret

_VERBOSE_RESET:
    xor a
    call WK_SET_STORAGE_DEV_FLAGS
    call WK_SET_MISC_FLAGS

    call HW_TEST
    ld hl,NOHARD_S
    jp c,PRINT

    ;Check for a FDD first. HWF_MOUNT_DISK makes the CH376 send mass storage
    ;(Bulk-Only) commands to the device, and some FDDs don't handle these well.

    push iy ;Buffer for the device name, USB_INIT_DEV modifies IY
    call _VERBOSE_RESET_INIT_FDD
    pop iy
    ld b,h  ;B = USB error code (must be saved before HL is modified)
    or a
    jp z,_VERBOSE_RESET_FDD_OK
    cp 4
    ld hl,NODEV_S
    jp z,PRINT

    ;* Not a FDD, or error when initializing it: check if it's a storage device

    push bc ;B = USB error code
    push af ;A = FDD init result
    call HW_RESET   ;Let the CH376 enumerate the device from scratch
    push iy
    pop hl
    push hl
    call HWF_MOUNT_DISK
    pop hl
    jr c,_VERBOSE_RESET_NO_STOR
    pop af
    pop bc

    push hl
    ld hl,STOR_FOUND_S
    call PRINT
    pop hl
    ld b,0
    ;Print the device name, collapsing multiple spaces to a single one
_HW_RESET_PRINT:
    ld a,(hl)
    inc hl
    or a
    jr z,_HW_RESET_PRINT_END
    cp ' '
    jr nz,_HW_RESET_PRINT_GO
    cp b
    jr z,_HW_RESET_PRINT
_HW_RESET_PRINT_GO:
    ld b,a
    call CHPUT
    jr _HW_RESET_PRINT
_HW_RESET_PRINT_END:
    call DSK_INIT_WK_FOR_STORAGE_DEV
    ret

    ;* Neither a FDD nor a storage device: print the appropriate error

_VERBOSE_RESET_NO_STOR:
    pop af
    pop bc
    dec a
    ld hl,NO_CBI_DEV_S
    jp z,PRINT
    dec a
    ld hl,RESERR_S
    jp nz,PRINT

    push bc
    ld hl,DEVERR_S
    call PRINT
    ld a,(USB_INIT_STEP)
    add "0"
    call CHPUT
    pop af  ;A = USB error code
    ld hl,DEVERR_STEP_S
    call PRINT_ERROR

    ;Diagnostics: raw CH376 status of the failed operation, and NAKs retried.
    ;If EP0_DIAGNOSTICS=1, for steps 4 and 5 also the status of single packet
    ;GET_DESCRIPTOR requests at address 1 and at address 0, and the endpoint 0 experiments.

    ld hl,DEVERR_STATUS_S
    call PRINT
    ld a,(USB_INIT_LAST_STATUS)
    call PRINT_HEX
    ld hl,DEVERR_NAKS_S
    call PRINT
    ld a,(USB_INIT_NAK_COUNT+1)
    call PRINT_HEX
    ld a,(USB_INIT_NAK_COUNT)
    call PRINT_HEX

    if EP0_DIAGNOSTICS = 1
    ld a,(USB_INIT_STEP)
    cp 4
    jr c,_VERBOSE_RESET_DIAG_END
    cp 6
    jr nc,_VERBOSE_RESET_DIAG_END
    ld hl,DEVERR_PROBE1_S
    call PRINT
    ld a,(USB_PROBE_ADDR1_STATUS)
    call PRINT_HEX
    ld hl,DEVERR_PROBE0_S
    call PRINT
    ld a,(USB_PROBE_ADDR0_STATUS)
    call PRINT_HEX
    ld hl,CRLF_S
    call PRINT
    call DIAG_EP0
_VERBOSE_RESET_DIAG_END:
    endif

    ld hl,CRLF_S
    jp PRINT

_VERBOSE_RESET_FDD_OK:
    call WK_GET_MISC_FLAGS
    and 1
    ld hl,HUB_FOUND_S
    call nz,PRINT

    ld hl,YES_CBI_DEV_S
    call PRINT
    jp PRINT_DEVICE_INFO

    ;--- Reset the USB hardware and try to initialize the device as a FDD
    ;    Output: A = 0: Ok, device is a FDD
    ;                1: Device is not a FDD
    ;                2: Error when initializing the device, H = USB error code
    ;                3: Error when resetting the USB hardware
    ;                4: No device connected

_VERBOSE_RESET_INIT_FDD:
    ld b,5
_HW_RESET_TRY:
    push bc
    call HW_RESET
    pop bc
    jr nc,_HW_RESET_TRY_OK
    djnz _HW_RESET_TRY
    ld a,3
    ret
_HW_RESET_TRY_OK:
    inc a
    ld a,4
    ret z

    ld b,5
_TRY_USB_INIT_DEV:
    push bc
    call USB_INIT_DEV
    ld h,b
    pop bc
    cp 2
    ret c
    push hl
    push bc
    call HW_BUS_RESET   ;Get the device back to the default state before retrying
    pop bc
    pop hl
    djnz _TRY_USB_INIT_DEV
    ld a,2
    ret

    if WAIT_KEY_ON_INIT = 1
INIHRD_NEXT:
    jp CHGET
    endif

PRINT:
	ld a,(hl)
	or a
	ret z
	call CHPUT
	inc hl
	jr PRINT

    ;Print A as a two digit hexadecimal number

PRINT_HEX:
    push af
    rrca
    rrca
    rrca
    rrca
    call _PRINT_HEX_DIGIT
    pop af
_PRINT_HEX_DIGIT:
    and 0Fh
    add "0"
    cp "9"+1
    jr c,_PRINT_HEX_DIGIT_2
    add "A"-"9"-1
_PRINT_HEX_DIGIT_2:
    jp CHPUT


; -----------------------------------------------------------------------------
; Print the device name from INQUIRY command

PRINT_DEVICE_INFO_STACK_SPACE: equ 36

PRINT_DEVICE_INFO:
    ld hl,-PRINT_DEVICE_INFO_STACK_SPACE
    add hl,sp
    ld sp,hl

    ld b,3  ;Some drives stall on first command after reset so try a few times
_TRY_INQUIRY:    
    push bc
    push hl
    pop de
    ld hl,INIQUIRY_CMD
    ld bc,36
    ld a,1
    or a
    push de
    call USB_EXECUTE_CBI_WITH_RETRY
    pop hl
    pop bc
    or a
    jr z,_INQUIRY_OK
    djnz _TRY_INQUIRY
    jr _PRINT_DEVICE_INFO_ERR
_INQUIRY_OK:

    ld bc,8
    add hl,bc
    ld b,8
    call PRINT_SPACE_PADDED_STRING
    ld a,' '
    call CHPUT

    ld bc,8 ;base + 16
    add hl,bc
    ld b,16
    call PRINT_SPACE_PADDED_STRING
    ld a,' '
    call CHPUT

    ld bc,16 ;base + 32
    add hl,bc
    ld b,4
    call PRINT_SPACE_PADDED_STRING

    ld hl,CRLF_S
    call PRINT

    jr _PRINT_DEVICE_INFO_END

_PRINT_DEVICE_INFO_ERR:
    ld hl,ERR_INQUIRY_S
    call PRINT_ERROR
_PRINT_DEVICE_INFO_END:    
    ld hl,PRINT_DEVICE_INFO_STACK_SPACE
    add hl,sp
    ld sp,hl
    ret

INIQUIRY_CMD:
    db 12h, 0, 0, 0, 36, 0, 0, 0, 0, 0, 0, 0


    ; Print a fixed-length space-padded string, skipping the padding
    ; Input: HL = String address
    ;        B  = String length

PRINT_SPACE_PADDED_STRING:
    push hl
    call _PRINT_SPACE_PADDED_STRING
    pop hl
    ret

_PRINT_SPACE_PADDED_STRING:
    ld e,b
    ld d,0
    push hl
    add hl,de   ;HL points past the last char of the string
_PSPS_Z_LOOP:
    dec hl
    ld a,(hl)
    cp ' '
    jr nz,_PSPS_DO
    djnz _PSPS_Z_LOOP
    pop hl
    ret         ;All the string is spaces, do nothing

_PSPS_DO:
    pop hl
_PSPS_P_LOOP:
    ld a,(hl)
    call CHPUT
    inc hl
    djnz _PSPS_P_LOOP

    ret


; -----------------------------------------------------------------------------
; Print an error message and the description of an USB error code
;
; Input: HL = Error message
;        A = Error code

PRINT_ERROR:
    push af
    call PRINT
    pop af

    ld ix,PRINT_ERROR_DESCRIPTION
    ld iy,ROM_BANK_0
    call CALL_BANK

    ld hl,CRLF_S
    jp PRINT


; -----------------------------------------------------------------------------
; Strings

ROOKIE_S:
	db "USBHOST NestorBIOS v2.2",13,10
	db "(c) 2018-2022 Konamiman",13,10
    db "(c) 2026 The Retro Hacker",13,10
	db 13,10
    db "Initializing device...",13
	db 0

NOHARD_S:
    db  "USB host hardware not found!"
CRLF_S:
    db 13,10,0

NODEV_S:
    db  "No USB device found",27,"K",13,10,0

NO_CBI_DEV_S:
    db  "USB device found, but it's not a FDD unit",13,10,0

YES_CBI_DEV_S:
    db  "USB FDD found: ",27,"K",0    

RESERR_S:
    db  "ERROR initializing USB host hardware or resetting USB device",13,10,0

DEVERR_S:
    db  "ERROR querying or initializing USB device (step ",0

DEVERR_STEP_S:
    db  "): ",0

DEVERR_STATUS_S:
    db  "CH376 status: ",0

DEVERR_NAKS_S:
    db  ", NAKs: ",0

    if EP0_DIAGNOSTICS = 1

DEVERR_PROBE1_S:
    db  ", probe @1: ",0

DEVERR_PROBE0_S:
    db  ", @0: ",0

    endif

ERR_INQUIRY_S:
    db  "ERROR querying the device name: ",0

STOR_FOUND_S:
    db "USB storage device found: ",0

HUB_FOUND_S:
    db "USB hub found",27,"K",13,10,0
