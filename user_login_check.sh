#!/bin/bash

# Check for users logged in on lab-clients.
# Not completely definitive, but good enough.
# saleem, 2022-02-02, 2021-08-01

# Assumes you have an SSH key already set-up on your host server, else
# you will get a login prompt for each remote machine!
# If you have an SSH key set-up on your host server, and run this script
# from your host server, things should just work ...

# Machine names screen-scraped from the URL below on 2022-02-02 12:11.
# pc5 machines are not included : they are to be used for GPU work only.
# pc7 machines and pc8 machines are generally usable.
# https://systems.wiki.cs.st-andrews.ac.uk/index.php/Lab_PCs#Remotely_Accessible_Linux_Clients


# ssh timeout values ...
t=8000000000 # nanoseconds, use with "date +%N", arbitrary choice of value.
t_t=$((t / 1000000000)) # seconds for use with timeout


lab_clients_test=(pc7-003-l pc7-004-l pc7-005-l pc7-006-l pc7-011-l pc7-012-l pc7-013-l pc7-014-l pc7-017-l pc7-018-l)


all_usable_lab_clients=(pc7-003-l pc7-004-l pc7-005-l pc7-006-l pc7-011-l pc7-012-l pc7-013-l pc7-014-l pc7-017-l pc7-018-l pc7-020-l pc7-023-l pc7-025-l pc7-026-l pc7-027-l pc7-028-l pc7-029-l pc7-036-l pc7-039-l pc7-043-l pc7-045-l pc7-050-l pc7-052-l pc7-054-l pc7-055-l pc7-056-l pc7-057-l pc7-059-l pc7-062-l pc7-063-l pc7-064-l pc7-066-l pc7-067-l pc7-070-l pc7-071-l pc7-073-l pc7-074-l pc7-075-l pc7-076-l pc7-078-l pc7-082-l pc7-085-l pc7-087-l pc7-089-l pc7-091-l pc7-092-l pc7-093-l pc7-094-l pc7-097-l pc7-098-l pc7-100-l pc7-103-l pc7-106-l pc7-109-l pc7-112-l pc7-113-l pc7-114-l pc7-116-l pc7-117-l pc7-119-l pc7-120-l pc7-121-l pc7-122-l pc7-123-l pc7-129-l pc7-133-l pc7-134-l pc7-135-l pc7-136-l pc7-139-l pc7-140-l pc7-145-l pc7-146-l pc7-147-l pc7-148-l pc7-149-l pc7-150-l pc8-001-l pc8-002-l pc8-003-l pc8-004-l pc8-005-l pc8-006-l pc8-007-l pc8-008-l pc8-009-l pc8-010-l pc8-011-l pc8-012-l pc8-013-l pc8-014-l pc8-015-l pc8-016-l pc8-017-l pc8-018-l pc8-019-l pc8-020-l pc8-021-l pc8-022-l pc8-023-l pc8-024-l pc8-025-l pc8-026-l pc8-027-l pc8-028-l pc8-029-l pc8-030-l pc8-031-l pc8-032-l pc8-033-l pc8-034-l pc8-035-l pc8-036-l pc8-037-l pc8-038-l pc8-039-l pc8-040-l pc8-041-l pc8-042-l pc8-043-l pc8-044-l pc8-045-l pc8-046-l pc8-047-l pc8-048-l pc8-049-l pc8-050-l pc8-051-l pc8-052-l pc8-053-l pc8-054-l pc8-055-l pc8-056-l pc8-057-l pc8-058-l pc8-059-l pc8-060-l pc8-061-l pc8-062-l pc8-063-l pc8-064-l pc8-066-l pc8-067-l pc8-068-l pc8-069-l pc8-070-l pc8-071-l pc8-072-l pc8-073-l pc8-074-l pc8-075-l pc8-076-l pc8-077-l pc8-078-l)

no_users_msg="--"
timeout_msg="(timeout: either booted in windows or possibly down)"

#names=(${lab_clients_test[*]})
names=(${all_usable_lab_clients[*]})

up=0
down=0
free=0
m=${#names[@]}
now=$(date)

printf 'Started on %s.\n' "$now"
printf '%s lab hosts to check ...\n' $m

start=$(date +%s)
for n in "${names[@]}"
do
  printf '%4s %6s ' $m $n

  e0=$(date +%s%N)
  users=$(timeout $t_t ssh $n.cs.st-andrews.ac.uk users)
  e1=$(date +%s%N)
  e=$((e1 - e0))

  if [ $e -gt $t ] # timeout - ssh failed
  then
    down=$((down + 1))
    printf ' %s' $timeout_msg
  else
    up=$((up + 1))
    if [ -z "$users" ]
    then
      free=$((free + 1))
      printf ' %s' $no_users_msg
    else
      printf ' %s' $users
    fi
  fi

  printf ' \n'
  m=$((m - 1))
done


printf '\n'
printf '>---- ---- ---- ----\n'
printf ' %4s machines in total checked.\n' ${#names[@]}
printf ' %4s machines available.\n' $up
printf ' %4s machines not available (ssh timeout after %ss).\n' $down $t_t
printf ' %4s machines have no users logged in.\n' $free
printf '>---- ---- ---- ----\n'

finish=$(date +%s)
printf '\nFinished in %s seconds.\n' $((finish - start))

