#!/usr/bin/env python3

import struct
import os
from decimal import *
from sys import argv
import sys
import csv
import argparse

class FileData:
	# Length of the ELF is used as the key
	table_info = {
		0x176C2C: {
			# Tested working
			'name': 'MAX JP',
			'offset': 0x175B18,
			'num_entries': 374
		},
		0x19D568: {
			# Tested working
			'name': 'MAX US',
			'offset': 0x168320,
			'num_entries': 577
		},
		0x1DAC88: {
			'name': 'MAX 2 JP',
			'offset': 0x17DDE8,
			'num_entries': 675
		},
		0x265854: {
			'name': 'MAX 2 US',
			'offset': 0x1A0810,
			'num_entries': 795
		},
		0x12A608: {
			'name': 'MAX 2 E3 Demo US',
			'offset': 0x1842F0,
			'num_entries': 223
		},
		2672124: {
			# Tested working
			'name': 'Extreme JP',
			'offset': 0x1B3130,
			'num_entries': 656
		},
		3871008: {
			'name': 'Extreme E3 Demo US',
			'offset': 0x17C880,
			'num_entries': 680
		},
		2725576: {
			'name': 'Party Collection JP',
			'offset': 0x1A1548,
			'num_entries': 459
		}
	}

	csv_fieldnames = ['id', 'offset', 'length', 'filename', 'description']

	def _get_table_info(self, elf_path):
		length = os.path.getsize(elf_path)
		try:
			return self.table_info[length]
		except:
			return None

	def _guess_filetype(self, stream, length):
		''' Guess the filetype of a binary stream
		
		Parameters:
		stream: file stream at start of data
		length: length of data
		
		Return value:
		Dictionary with description and extension keys.
		The file pointer will be returned to its original position.
		'''

		if length > 4 and stream.peek(4)[:4] == b'Svag':
			return {'description': 'Konami PS2 SVAG Audio', 'extension': 'svag'}
		elif length > 4 and stream.peek(4)[:4] == b'ipum':
			return {'description': 'Sony PS2 IPU Video', 'extension': 'ipu'}
		elif length > 16 and stream.peek(16)[:16] == b'TCB\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00':
			return {'description': 'Konami TCB Image', 'extension': 'tcb'}
		elif length > 13 and stream.peek(13)[4:13] == b'FrameInfo':
			return {'description': 'MPEG2 Stream', 'extension': 'mpeg2'}
		else:
			return {'description': 'Unknown Binary Data', 'extension': 'bin'}

	def _print_progress(self, iteration, total):
		''' Print progress info to stdout '''
		percent = int(round(100 * iteration/total, 0))
		bar_filled = percent // 10
		bar = '#' * bar_filled + '-' * (10 - bar_filled)
		sys.stdout.write('\r[{}] {}%'.format(bar, percent))
		sys.stdout.flush()

	def __init__(self, elf_path, filedata_path):
		self.elf_path = elf_path
		self.filedata_path = filedata_path
		self.table = []
		self.table_info = self._get_table_info(elf_path)
		if self.table_info == None:
			raise LookupError('Unknown ELF; Cannot extract filetable.')
		
	def load_from_elf(self):
		''' Read table contents from ELF file. '''
		filedata_file = open(self.filedata_path, 'rb')
		with open(self.elf_path, 'rb') as elf_file:
			elf_file.seek(self.table_info['offset'], os.SEEK_SET)
			for _ in range(self.table_info['num_entries']):
				data = struct.unpack('<HBBBBBB', elf_file.read(8))
				id = data[0]
				offset = int.from_bytes([data[1],data[2],data[3]], byteorder='little', signed=False) * 0x800
				length = int.from_bytes([data[4],data[5],data[6]], byteorder='little', signed=False) * 0x800
				
				filedata_file.seek(offset)
				filetype = self._guess_filetype(filedata_file, length)
				filename = '{}.{}'.format(id, filetype['extension'])

				self.table.append({
					'id': data[0],
					'offset': offset,
					'length': length,
					'description': filetype['description'],
					'filename': filename
				})

		# Look for hidden data not referenced in the ELF.
		table_sorted = list(sorted(self.table, key=lambda k: k['offset']))
		for i in range(len(table_sorted) - 1):
			expected_next_offset = table_sorted[i]['offset'] + table_sorted[i]['length']
			if not table_sorted[i+1]['offset'] == expected_next_offset:
				print('Found hidden data at {}'.format(expected_next_offset))
				hidden_length = table_sorted[i+1]['offset'] - expected_next_offset
				filedata_file.seek(expected_next_offset)
				hidden_filetype = self._guess_filetype(filedata_file, hidden_length)
				hidden_filename = 'hidden_{}.{}'.format(expected_next_offset, hidden_filetype['extension'])
				self.table.append({
					'id': 'hidden',
					'offset': expected_next_offset,
					'length': hidden_length,
					'description': hidden_filetype['description'],
					'filename': hidden_filename
				})

		if not table_sorted[-1]['offset'] + table_sorted[-1]['length'] == os.path.getsize(self.filedata_path):
			# Found hidden data at the end of filedata.bin not listed in the ELF.
			print('Found hidden data at {}'.format(expected_next_offset))
			hidden_length = os.path.getsize(self.filedata_path) - (table_sorted[-1]['offset'] + table_sorted[-1]['length'])
			hidden_offset = os.path.getsize(self.filedata_path) - hidden_length
			filedata_file.seek(hidden_offset)
			hidden_filetype = self._guess_filetype(filedata_file, hidden_length)
			hidden_filename = 'hidden_{}.{}'.format(hidden_offset, hidden_filetype['extension'])
			self.table.append({
				'id': 'hidden',
				'offset': hidden_offset,
				'length': hidden_length,
				'description': hidden_filetype['description'],
				'filename': hidden_filename
			})

	def save_to_elf(self):
		''' Update table contents in ELF file '''
		table = list(filter(lambda x: not x['id'] == 'hidden', self.table))
		if not self.table_info['num_entries'] == len(table):
			raise Exception('ELF only supports {} entries but loaded table has {} entries.'.format(
				self.table_info['num_entries'], len(table)
			))
		with open(self.elf_path, 'rb+') as elf_file:
			elf_file.seek(self.table_info['offset'], os.SEEK_SET)
			for row in table:
				elf_file.write(struct.pack('<H', int(row['id'])))
				elf_file.write(struct.pack('<I', int(row['offset']) >> 3)[1:])
				elf_file.write(struct.pack('<I', int(row['length']) >> 3)[1:])

	def load_from_csv(self, csv_path):
		''' Read table contents from CSV file. '''
		with open(csv_path, 'r') as csv_file:
			self.table = list(csv.DictReader(csv_file))
			
	def save_to_csv(self, csv_path):
		''' Save table contents to CSV file. '''
		os.makedirs(os.path.dirname(csv_path), exist_ok=True)
		with open(csv_path, 'w') as csv_file:
			csv_writer = csv.DictWriter(csv_file, fieldnames=self.csv_fieldnames)
			csv_writer.writeheader()
			csv_writer.writerows(self.table)

	def extract(self, output_directory):
		''' Extract contents of a filedata.bin file. '''
		os.makedirs(output_directory, exist_ok=True)
		with open(self.filedata_path, 'rb') as filedata:
			for i, row in enumerate(self.table):
				self._print_progress(i, len(self.table))
				with open(os.path.join(output_directory, row['filename']), 'wb') as out:
					filedata.seek(row['offset'], os.SEEK_SET)
					out.write(filedata.read(row['length']))
		print('')
	
	def create(self, input_directory):
		''' Create filedata.bin file, updating the internal table if file sizes have changed. '''
		sorted_indices = [i[0] for i in sorted(enumerate(self.table), key=lambda x: int(x[1]['offset']))]
		with open(self.filedata_path, 'wb') as filedata:
			for i in sorted_indices:
				self._print_progress(i, len(self.table))
				file_path = os.path.join(input_directory, self.table[i]['filename'])
				self.table[i]['length'] = os.path.getsize(file_path)
				self.table[i]['length'] += self.table[i]['length'] % 0x800
				with open(file_path, 'rb') as input:
					filedata.write(input.read())
					filedata.write(bytearray(self.table[i]['length'] % 0x800))
		
		# Update offsets to account for potentially updated lengths
		offset = 0
		for i in sorted_indices:
			self.table[i]['offset'] = offset
			offset += self.table[i]['length']

		writer = csv.DictWriter(open('filedata_sorted.csv', 'w'), fieldnames=self.csv_fieldnames)
		writer.writeheader()
		writer.writerows(self.table)
		print('')

def print_usage(filename):
	print('Usage:')
	print('Extract filedata.bin using file table in ELF:')
	print('    {} extract <elf> <filedata> <output directory>'.format(filename))
	print('Create filedata.bin and update file table in ELF:')
	print('    {} create <elf> <filedata> <input directory>'.format(filename))

if not len(argv) == 5:
	print_usage(argv[0])
	sys.exit()

elf_path = os.path.abspath(argv[2])
filedata_path = os.path.abspath(argv[3])
directory = os.path.abspath(argv[4])
csv_path = os.path.join(directory, 'filedata.csv')

filedata = FileData(elf_path, filedata_path)

if argv[1] == 'extract':
	filedata.load_from_elf()
	filedata.save_to_csv(csv_path)
	filedata.extract(directory)
elif argv[1] == 'create':
	filedata.load_from_csv(csv_path)
	filedata.create(directory)
	filedata.save_to_elf()
else:
	print('Unknown action \'{}\''.format(argv[1]))
	exit(1)