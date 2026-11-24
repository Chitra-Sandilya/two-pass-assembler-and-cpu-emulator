
;Name:    Chitra Sandilya
;Roll Number: 2501CS15

;I, declare that this source file is my own work.

; bubble sort

N:      SET 5

start:
    adj -5          ; reserve 5 stack slots for the local variables (SP starts at top of memory)
    ldc N            ; A=5

    adc -1          ;A=5-1=4
    stl 0           ; stack slot 0: outer loop counter i, STACK SLOT 1:i, i=4

outerloop:
    ldl 0
    brlz endsort    ; if i < 0, sorting is complete

    ldc 0
    stl 1           ; stack slot 1: inner loop counter j

innerloop:
    ldl 0           ; A = i
    ldl 1           ; B = i, A = j
    sub             ; A = i - j
    brz endinner    ; if i == j, exit inner loop
    brlz endinner   ; if i < j, exit inner loop

    ldc arr
    ldl 1
    add
    ldnl 0
    stl 2           ; slot 2 = arr[j]

    ldc arr
    ldl 1
    add
    adc 1
    ldnl 0
    stl 3           ; slot 3 = arr[j+1]

    ldl 2           ; A = arr[j]
    ldl 3           ; B = arr[j], A = arr[j+1]
    sub             ; A = arr[j] - arr[j+1]
    brlz noswap     ; if arr[j] < arr[j+1], no swap
    brz noswap      ; if arr[j] == arr[j+1], no swap

    ldc arr
    ldl 1
    add
    stl 4           ; slot 4 = address of arr[j]
    ldl 3           ; A = arr[j+1]
    ldl 4           ; B = arr[j+1], A = address
    stnl 0          ; arr[j] = arr[j+1]

    ldc arr
    ldl 1
    add
    adc 1
    stl 4           ; slot 4 = address of arr[j+1]
    ldl 2           ; A = arr[j]
    ldl 4           ; B = arr[j], A = address
    stnl 0          ; arr[j+1] = arr[j]

noswap:
    ldl 1
    adc 1
    stl 1
    br innerloop

endinner:
    ldl 0
    adc -1
    stl 0
    br outerloop

endsort:
    adj 5           ; release the stack slots
    halt

arr:    data 34
        data 12
        data 54
        data 2
        data 18